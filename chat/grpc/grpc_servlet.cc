#include "grpc_servlet.h"
#include "chat/log.h"

namespace chat {
namespace grpc {

static chat::Logger::ptr g_logger = CHAT_LOG_NAME("system");

std::string GrpcServlet::GetGrpcPath(const std::string& ns,
                    const std::string& service, const std::string& method) {
    std::string path = "/";
    if(!ns.empty()) {
        path += ns + ".";
    }
    path += service + "/" + method;
    return path;
}

GrpcServlet::GrpcServlet(const std::string& name)
    :chat::http::Servlet(name) {
}

int32_t GrpcServlet::handle(chat::http::HttpRequest::ptr request
                   , chat::http::HttpResponse::ptr response
                   , chat::SocketStream::ptr session) {
    std::string content_type = request->getHeader("content-type");
    if(content_type.empty()
            || content_type.find("application/grpc") == std::string::npos) {
        response->setStatus(chat::http::HttpStatus::UNSUPPORTED_MEDIA_TYPE);
        return 0;
    }

    response->setHeader("content-type", content_type);
    auto trailer = chat::StringUtil::Trim(request->getHeader("te"));
    if(trailer == "trailers") {
        response->setHeader("trailer", "grpc-status,grpc-message");
    }

    auto& body = request->getBody();
    if(body.size() < 5) {
        response->setStatus(chat::http::HttpStatus::BAD_REQUEST);
        response->setReason("body less 5");
        return 0;
    }

    chat::ByteArray::ptr ba(new chat::ByteArray((void*)&body[0], body.size()));
    GrpcMessage::ptr msg = std::make_shared<GrpcMessage>();
    try {
        msg->compressed = ba->readFuint8();
        msg->length = ba->readFuint32();
        msg->data = ba->toString();
    } catch (...) {
        CHAT_LOG_ERROR(g_logger) << "invalid grpc body";
        response->setStatus(chat::http::HttpStatus::BAD_REQUEST);
        response->setReason("invalid grpc body");
        return -1;
    }

    GrpcRequest::ptr greq = std::make_shared<GrpcRequest>();
    greq->setData(msg);
    greq->setRequest(request);

    GrpcResult::ptr grsp = std::make_shared<GrpcResult>(0, "", 0);
    grsp->setResponse(response);
    int rt = process(greq, grsp, session);
    if(rt == 0) {
        response->setHeader("grpc-status", std::to_string(grsp->getResult()));
        response->setHeader("grpc-message", grsp->getError());

        auto grpc_data = grsp->getData();
        if(grpc_data) {
            std::string data;
            data.resize(grpc_data->data.size() + 5);
            chat::ByteArray::ptr ba(new chat::ByteArray(&data[0], data.size()));
            ba->writeFuint8(0);
            ba->writeFuint32(grpc_data->data.size());
            ba->write(grpc_data->data.c_str(), grpc_data->data.size());
            response->setBody(data);
        }
    }
    return rt;
}


GrpcFunctionServlet::GrpcFunctionServlet(callback cb)
    :GrpcServlet("GrpcFunctionServlet")
    ,m_cb(cb) {
}

int32_t GrpcFunctionServlet::process(chat::grpc::GrpcRequest::ptr request,
                                     chat::grpc::GrpcResult::ptr response,
                                     chat::SocketStream::ptr session) {
    return m_cb(request, response, session);
}

GrpcFunctionServlet::ptr GrpcFunctionServlet::Create(callback cb) {
    return std::make_shared<GrpcFunctionServlet>(cb);
}

}
}