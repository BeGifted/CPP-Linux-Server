#include "grpc_servlet.h"
#include "chat/log.h"
#include "chat/streams/zlib_stream.h"
#include "grpc_util.h"

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

GrpcServlet::GrpcServlet(const std::string& name, GrpcType type)
    :chat::http::Servlet(name)
    ,m_type(type) {
}

int32_t GrpcServlet::process(chat::grpc::GrpcRequest::ptr request,
                            chat::grpc::GrpcResponse::ptr response,
                            chat::grpc::GrpcSession::ptr session) {
    CHAT_LOG_WARN(g_logger) << "GrpcServlet name=" << m_name << " unhandle unary process";
    session->sendRstStream(request->getRequest()->getStreamId(), 404);
    return -1;
}

int32_t GrpcServlet::processStream(chat::grpc::GrpcRequest::ptr request,
                            chat::grpc::GrpcResponse::ptr response,
                            chat::grpc::GrpcStream::ptr stream,
                            chat::grpc::GrpcSession::ptr session) {
    CHAT_LOG_WARN(g_logger) << "GrpcServlet name=" << m_name << " unhandle stream processStream type=" << (uint32_t)m_type;
    session->sendRstStream(request->getRequest()->getStreamId(), 404);
    return -1;
}

int32_t GrpcServlet::handle(chat::http::HttpRequest::ptr request
                   , chat::http::HttpResponse::ptr response
                   , chat::SocketStream::ptr session) {
    CHAT_LOG_INFO(g_logger) << "GrpcServlet handle " << request->getPath();
    std::string content_type = request->getHeader(CONTENT_TYPE);
    if(content_type.empty()
            || content_type.find("application/grpc") == std::string::npos) {
        response->setStatus(chat::http::HttpStatus::UNSUPPORTED_MEDIA_TYPE);
        return 1;
    }

    response->setHeader(CONTENT_TYPE, content_type);
    auto trailer = chat::StringUtil::Trim(request->getHeader("te"));
    if(trailer == "trailers") {
        response->setHeader("trailer", "grpc-status,grpc-message");
    }

    GrpcMessage::ptr msg = std::make_shared<GrpcMessage>();
    chat::grpc::GrpcStream::ptr cli;
    chat::grpc::GrpcSession::ptr h2session = std::dynamic_pointer_cast<chat::grpc::GrpcSession>(session);

    if(m_type == GrpcType::UNARY) {
        auto& body = request->getBody();
        CHAT_LOG_INFO(g_logger) << "==== body.size=" << body.size();
        if(body.size() < 5) {
            response->setHeader(GRPC_STATUS, "100");
            response->setHeader(GRPC_MESSAGE, "body less 5");
            response->setStatus(chat::http::HttpStatus::BAD_REQUEST);
            response->setReason("body less 5");
            return 1;
        }

        if(!DecodeGrpcBody(body, msg->data)) {
            CHAT_LOG_ERROR(g_logger) << "invalid grpc body";
            response->setStatus(chat::http::HttpStatus::BAD_REQUEST);
            response->setReason("invalid grpc body");
            return -1;
        }
    } else {
        auto stream_id = request->getStreamId();
        auto stream = h2session->getStream(stream_id);
        cli = std::make_shared<chat::grpc::GrpcStream>(stream);
        if(m_type == GrpcType::SERVER) {
            auto data = cli->recvMessageData();
            if(data) {
                msg->data = *data;
                msg->length = msg->data.size();
                msg->compressed = 0;
            } else {
                CHAT_LOG_ERROR(g_logger) << "invalid grpc body";
                response->setStatus(chat::http::HttpStatus::BAD_REQUEST);
                response->setReason("invalid grpc body");
                return -1;
            }
        }
    }

    GrpcRequest::ptr greq = std::make_shared<GrpcRequest>();
    greq->setData(msg);
    greq->setRequest(request);

    GrpcResponse::ptr grsp = std::make_shared<GrpcResponse>(0, "", 0);
    grsp->setResponse(response);

    m_request = greq;
    m_response = grsp;
    m_session = h2session;
    m_stream = cli;

    int rt = 0;
    if(m_type == GrpcType::UNARY) {
        rt = process(greq, grsp, h2session);
    } else {
        if(request->getHeader(GRPC_ACCEPT_ENCODING) == "gzip") {
            response->setHeader(GRPC_ENCODING, "gzip");
            cli->setEnableGzip(true);
        }
        if(m_type != GrpcType::CLIENT) {
            CHAT_LOG_INFO(g_logger) << "**** rsp: " << *response;
            cli->getStream()->sendResponse(grsp->getResponse(), false, true);
            auto& headers = response->getHeaders();
            for(auto& i : headers) {
                CHAT_LOG_INFO(g_logger) << i.first << " - " << i.second;
            }
        }
        CHAT_LOG_INFO(g_logger) << "=== stream type=" << (uint32_t)m_type;
        rt = processStream(greq, grsp, cli, h2session);
    }
    if(rt == 0) {
        if(m_type == GrpcType::UNARY || m_type == GrpcType::CLIENT) {
            response->setHeader(GRPC_STATUS, std::to_string(grsp->getResult()));
            response->setHeader(GRPC_MESSAGE, grsp->getError());
            auto grpc_data = grsp->getData();
            if(grpc_data) {
                //hardcode TODO
                chat::ByteArray::ptr ba;
                if(request->getHeader(GRPC_ACCEPT_ENCODING) == "gzip") {
                    response->setHeader(GRPC_ENCODING, "gzip");
                    ba = grpc_data->packData(true);
                } else {
                    ba = grpc_data->packData(false);
                }
                ba->setPosition(0);
                response->setBody(ba->toString());
            }
        } else {
            std::map<std::string, std::string> headers;
            headers[GRPC_STATUS] = std::to_string(grsp->getResult());
            headers[GRPC_MESSAGE] = grsp->getError();
            cli->getStream()->sendHeaders(headers, true, true);

            for(auto& i : headers) {
                CHAT_LOG_INFO(g_logger) << i.first << " : " << i.second;
            }
        }
    } else {
        response->setHeader(GRPC_STATUS, std::to_string(grsp->getResult()));
        response->setHeader(GRPC_MESSAGE, grsp->getError());
    }
    return rt;
}


GrpcFunctionServlet::GrpcFunctionServlet(GrpcType type, callback cb, stream_callback scb)
    :GrpcServlet("GrpcFunctionServlet", type)
    ,m_cb(cb)
    ,m_scb(scb) {
}

GrpcServlet::ptr GrpcFunctionServlet::clone() {
    return std::make_shared<GrpcFunctionServlet>(*this);
}

int32_t GrpcFunctionServlet::process(chat::grpc::GrpcRequest::ptr request,
                                     chat::grpc::GrpcResponse::ptr response,
                                     chat::grpc::GrpcSession::ptr session) {
    if(m_cb) {
        return m_cb(request, response, session);
    } else {
        session->sendRstStream(request->getRequest()->getStreamId(), 404);
        CHAT_LOG_WARN(g_logger) << "GrpcFunctionServlet unhandle process " << request->getRequest()->getPath();
        return -1;
    }
}

int32_t GrpcFunctionServlet::processStream(chat::grpc::GrpcRequest::ptr request,
                                           chat::grpc::GrpcResponse::ptr response,
                                           chat::grpc::GrpcStream::ptr stream,
                                           chat::grpc::GrpcSession::ptr session) {
    if(m_scb) {
        return m_scb(request, response, stream, session);
    } else {
        session->sendRstStream(request->getRequest()->getStreamId(), 404);
        CHAT_LOG_WARN(g_logger) << "GrpcFunctionServlet unhandle processStream " << request->getRequest()->getPath();
        return -1;
    }
}

GrpcFunctionServlet::ptr GrpcFunctionServlet::Create(GrpcType type, callback cb, stream_callback scb) {
    return std::make_shared<GrpcFunctionServlet>(type, cb, scb);
}

}
}
