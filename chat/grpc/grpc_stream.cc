#include "grpc_stream.h"
#include "chat/log.h"
#include "chat/worker.h"
#include <sstream>

namespace chat {
namespace grpc {

static chat::Logger::ptr g_logger = CHAT_LOG_NAME("system");

static chat::ConfigVar<std::unordered_map<std::string
    ,std::unordered_map<std::string, std::string> > >::ptr g_grpc_services =
    chat::Config::Lookup("grpc_services", std::unordered_map<std::string
    ,std::unordered_map<std::string, std::string> >(), "grpc_services");


std::string GrpcRequest::toString() const {
    std::stringstream ss;
    ss << "[GrpcRequest request=" << m_request
       << " data=" << m_data << "]";
    return ss.str();
}

std::string GrpcResult::toString() const {
    std::stringstream ss;
    ss << "[GrpcResult result=" << m_result
       << " error=" << m_error
       << " response=" << m_response << "]";
    return ss.str();
}

GrpcResult::ptr GrpcConnection::request(GrpcRequest::ptr req, uint64_t timeout_ms) {
    auto http_req = req->getRequest();
    http_req->setHeader("content-type", "application/grpc+proto");
    auto grpc_data = req->getData();
    std::string data;
    data.resize(grpc_data->data.size() + 5);
    chat::ByteArray::ptr ba(new chat::ByteArray(&data[0], data.size()));
    ba->writeFuint8(0);
    ba->writeFuint32(grpc_data->data.size());
    ba->write(grpc_data->data.c_str(), grpc_data->data.size());
    http_req->setBody(data);

    GrpcResult::ptr rsp = std::make_shared<GrpcResult>();
    auto result = Http2Connection::request(http_req, timeout_ms);
    rsp->setResult(result->result);
    rsp->setError(result->error);
    rsp->setResponse(result->response);
    if(!result->result) {
        rsp->setResult(chat::TypeUtil::Atoi(result->response->getHeader("grpc-status")));
        rsp->setError(result->response->getHeader("grpc-message"));

        auto& body = result->response->getBody();
        if(!body.empty()) {
            chat::ByteArray::ptr ba(new chat::ByteArray((void*)&body[0], body.size()));
            GrpcMessage::ptr msg = std::make_shared<GrpcMessage>();
            rsp->setData(msg);

            try {
                msg->compressed = ba->readFuint8();
                msg->length = ba->readFuint32();
                msg->data = ba->toString();
            } catch (...) {
                CHAT_LOG_ERROR(g_logger) << "invalid grpc body";
                result->result = -501;
                result->error = "invalid grpc body";
            }
        }
    }
    return rsp;
}

GrpcResult::ptr GrpcConnection::request(const std::string& method
        , const google::protobuf::Message& message
        , uint64_t timeout_ms
        , const std::map<std::string, std::string>& headers) {
    GrpcRequest::ptr grpc_req = std::make_shared<GrpcRequest>();
    http::HttpRequest::ptr http_req = std::make_shared<http::HttpRequest>(0x20, false);
    grpc_req->setRequest(http_req);
    http_req->setMethod(http::HttpMethod::POST);
    http_req->setPath(method);
    for(auto & i : headers) {
        http_req->setHeader(i.first, i.second);
    }
    grpc_req->setAsPB(message);
    return request(grpc_req, timeout_ms);
}

GrpcConnection::GrpcConnection() {
    //m_autoConnect = false;
}


GrpcSDLoadBalance::GrpcSDLoadBalance(IServiceDiscovery::ptr sd)
    :SDLoadBalance(sd) {
    m_type = "grpc";
}

static SocketStream::ptr create_grpc_stream(const std::string& domain, const std::string& service, ServiceItemInfo::ptr info) {
    chat::IPAddress::ptr addr = chat::Address::LookupAnyIPAddress(info->getIp());
    if(!addr) {
        CHAT_LOG_ERROR(g_logger) << "invalid service info: " << info->toString();
        return nullptr;
    }
    addr->setPort(info->getPort());
    GrpcConnection::ptr conn = std::make_shared<GrpcConnection>();

    chat::WorkerMgr::GetInstance()->schedule("service_io", [conn, addr, domain, service](){
        conn->connect(addr);
        CHAT_LOG_INFO(g_logger) << *addr << " - " << domain << " - " << service;
        conn->start();
    });
    return conn;
}

void GrpcSDLoadBalance::start() {
    m_cb = create_grpc_stream;
    initConf(g_grpc_services->getValue());
    SDLoadBalance::start();
}

void GrpcSDLoadBalance::start(const std::unordered_map<std::string
                              ,std::unordered_map<std::string,std::string> >& confs) {
    m_cb = create_grpc_stream;
    initConf(confs);
    SDLoadBalance::start();
}

void GrpcSDLoadBalance::stop() {
    SDLoadBalance::stop();
}

GrpcResult::ptr GrpcSDLoadBalance::request(const std::string& domain, const std::string& service,
                                           GrpcRequest::ptr req, uint32_t timeout_ms, uint64_t idx) {
    auto lb = get(domain, service);
    if(!lb) {
        return std::make_shared<GrpcResult>(ILoadBalance::NO_SERVICE, "no_service", 0);
    }
    auto conn = lb->get(idx);
    if(!conn) {
        return std::make_shared<GrpcResult>(ILoadBalance::NO_CONNECTION, "no_connection", 0);
    }
    uint64_t ts = chat::GetCurrentMs();
    auto& stats = conn->get(ts / 1000);
    stats.incDoing(1);
    stats.incTotal(1);
    auto r = conn->getStreamAs<GrpcConnection>()->request(req, timeout_ms);
    uint64_t ts2 = chat::GetCurrentMs();
    r->setUsed(ts2 - ts);
    if(r->getResult() == 0) {
        stats.incOks(1);
        stats.incUsedTime(ts2 -ts);
    } else if(r->getResult() == AsyncSocketStream::TIMEOUT) {
        stats.incTimeouts(1);
    } else if(r->getResult() < 0) {
        stats.incErrs(1);
    }
    stats.decDoing(1);
    return r;
}

GrpcResult::ptr GrpcSDLoadBalance::request(const std::string& domain, const std::string& service,
                                           const std::string& method, const google::protobuf::Message& message,
                                           uint32_t timeout_ms,
                                           const std::map<std::string, std::string>& headers,
                                           uint64_t idx) {
    auto lb = get(domain, service);
    if(!lb) {
        return std::make_shared<GrpcResult>(ILoadBalance::NO_SERVICE, "no_service", 0);
    }
    auto conn = lb->get(idx);
    if(!conn) {
        return std::make_shared<GrpcResult>(ILoadBalance::NO_CONNECTION, "no_connection", 0);
    }
    uint64_t ts = chat::GetCurrentMs();
    auto& stats = conn->get(ts / 1000);
    stats.incDoing(1);
    stats.incTotal(1);
    auto r = conn->getStreamAs<GrpcConnection>()->request(method, message, timeout_ms, headers);
    uint64_t ts2 = chat::GetCurrentMs();
    r->setUsed(ts2 - ts);
    if(r->getResult() == 0) {
        stats.incOks(1);
        stats.incUsedTime(ts2 -ts);
    } else if(r->getResult() == AsyncSocketStream::TIMEOUT) {
        stats.incTimeouts(1);
    } else if(r->getResult() < 0) {
        stats.incErrs(1);
    }
    stats.decDoing(1);
    return r;
}

}
}