#include "grpc_stream.h"
#include "chat/log.h"
#include <sstream>

namespace chat {
namespace grpc {

static chat::Logger::ptr g_logger = CHAT_LOG_NAME("system");

std::string GrpcRequest::toString() const {
    std::stringstream ss;
    ss << "[GrpcRequest request=" << m_request
       << " data=" << m_data << "]";
    return ss.str();
}

std::string GrpcResponse::toString() const {
    std::stringstream ss;
    ss << "[GrpcResponse result=" << m_result
       << " error=" << m_error
       << " response=" << m_response << "]";
    return ss.str();
}

GrpcResponse::ptr GrpcConnection::request(GrpcRequest::ptr req, uint64_t timeout_ms) {
    auto http_req = req->getRequest();
    http_req->setHeader("content-type", "application/grpc");
    auto grpc_data = req->getData();
    std::string data;
    data.resize(grpc_data->data.size() + 5);
    chat::ByteArray::ptr ba(new chat::ByteArray(&data[0], data.size()));
    ba->writeFuint8(0);
    ba->writeFuint32(grpc_data->data.size());
    ba->write(grpc_data->data.c_str(), grpc_data->data.size());
    http_req->setBody(data);

    GrpcResponse::ptr rsp = std::make_shared<GrpcResponse>();
    auto result = Http2Connection::request(http_req, timeout_ms);
    rsp->setResult(result->result);
    rsp->setError(result->error);
    rsp->setResponse(result->response);
    if(!result->result) {
        rsp->setResult(chat::TypeUtil::Atoi(result->response->getHeader("grpc-status")));
        rsp->setError(result->response->getHeader("grpc-message"));

        auto& body = result->response->getBody();
        chat::ByteArray::ptr ba(new chat::ByteArray((void*)&body[0], body.size()));
        GrpcMessage::ptr msg = std::make_shared<GrpcMessage>();
        rsp->setData(msg);

        msg->compressed = ba->readFuint8();
        msg->length = ba->readFuint32();
        msg->data = ba->toString();
    }
    return rsp;
}

GrpcResponse::ptr GrpcConnection::request(const std::string& method
        ,const google::protobuf::Message& message, uint64_t timeout_ms) {
    GrpcRequest::ptr grpc_req = std::make_shared<GrpcRequest>();
    http::HttpRequest::ptr http_req = std::make_shared<http::HttpRequest>(0x20, false);
    grpc_req->setRequest(http_req);
    http_req->setMethod(http::HttpMethod::POST);
    http_req->setPath(method);
    grpc_req->setAsPB(message);
    return request(grpc_req, timeout_ms);
}

GrpcConnection::GrpcConnection() {
    //m_autoConnect = false;
}

}
}