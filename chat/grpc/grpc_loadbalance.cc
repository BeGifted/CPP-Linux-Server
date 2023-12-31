#include "grpc_loadbalance.h"
#include "chat/log.h"
#include "chat/config.h"
#include "chat/worker.h"
#include "chat/streams/zlib_stream.h"
#include "grpc_util.h"
#include "grpc_connection.h"
#include <sstream>

namespace chat {
namespace grpc {

static chat::Logger::ptr g_logger = CHAT_LOG_NAME("system");

static chat::ConfigVar<std::unordered_map<std::string
    ,std::unordered_map<std::string, std::string> > >::ptr g_grpc_services =
    chat::Config::Lookup("grpc_services", std::unordered_map<std::string
    ,std::unordered_map<std::string, std::string> >(), "grpc_services");

GrpcSDLoadBalance::GrpcSDLoadBalance(IServiceDiscovery::ptr sd)
    :SDLoadBalance(sd) {
    m_type = "grpc";
}

static SocketStream::ptr create_grpc_stream(const std::string& domain, const std::string& service, ServiceItemInfo::ptr info) {
    //CHAT_LOG_INFO(g_logger) << "create_grpck_stream: " << info->toString();
    chat::IPAddress::ptr addr = chat::Address::LookupAnyIPAddress(info->getIp());
    if(!addr) {
        CHAT_LOG_ERROR(g_logger) << "invalid service info: " << info->toString();
        return nullptr;
    }
    addr->setPort(info->getPort());
    GrpcConnection::ptr conn = std::make_shared<GrpcConnection>();

    chat::WorkerMgr::GetInstance()->schedule("service_io", [conn, addr, domain, service](){
        conn->connect(addr);
        //CHAT_LOG_INFO(g_logger) << *addr << " - " << domain << " - " << service;
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

GrpcResponse::ptr GrpcSDLoadBalance::request(const std::string& domain, const std::string& service,
                                           GrpcRequest::ptr req, uint32_t timeout_ms, uint64_t idx) {
    auto lb = get(domain, service);
    if(!lb) {
        return std::make_shared<GrpcResponse>(ILoadBalance::NO_SERVICE, "no_service", 0);
    }
    auto conn = lb->get(idx);
    if(!conn) {
        return std::make_shared<GrpcResponse>(ILoadBalance::NO_CONNECTION, "no_connection", 0);
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

GrpcResponse::ptr GrpcSDLoadBalance::request(const std::string& domain, const std::string& service,
                                           const std::string& method, PbMessagePtr message,
                                           uint32_t timeout_ms,
                                           const std::map<std::string, std::string>& headers,
                                           uint64_t idx) {
    auto lb = get(domain, service);
    if(!lb) {
        return std::make_shared<GrpcResponse>(ILoadBalance::NO_SERVICE, "no_service", 0);
    }
    auto conn = lb->get(idx);
    if(!conn) {
        return std::make_shared<GrpcResponse>(ILoadBalance::NO_CONNECTION, "no_connection", 0);
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


GrpcStream::ptr GrpcSDLoadBalance::openGrpcStream(const std::string& domain,
                                   const std::string& service,
                                   const std::string& method,
                                   const std::map<std::string, std::string>& headers,
                                   uint64_t idx) {
    auto lb = get(domain, service);
    if(!lb) {
        return nullptr;
        //return std::make_shared<GrpcResponse>(ILoadBalance::NO_SERVICE, "no_service", 0);
    }
    auto conn = lb->get(idx);
    if(!conn) {
        //return std::make_shared<GrpcResponse>(ILoadBalance::NO_CONNECTION, "no_connection", 0);
        return nullptr;
    }

    return conn->getStreamAs<GrpcConnection>()->openGrpcStream(method, headers);
}


/*
GrpcStreamBase::GrpcStreamBase(GrpcStreamClient::ptr client)
        :m_client(client) {
}

int32_t GrpcStreamBase::close(int err) {
    if(!m_client) {
        return -1;
    }
    auto stm = m_client->getStream();
    if(!stm) {
        return -2;
    }
    auto hs = stm->getSockStream();
    if(!hs) {
        return -3;
    }
    return hs->sendRstStream(stm->getId(), err);
}
*/

}
}
