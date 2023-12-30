#ifndef __CHAT_GRPC_SERVLET_H__
#define __CHAT_GRPC_SERVLET_H__

#include "chat/http/servlet.h"
#include "grpc_stream.h"

namespace chat {
namespace grpc {

class GrpcServlet : public chat::http::Servlet {
public:
    typedef std::shared_ptr<GrpcServlet> ptr;
    GrpcServlet(const std::string& name);
    int32_t handle(chat::http::HttpRequest::ptr request
                   , chat::http::HttpResponse::ptr response
                   , chat::SocketStream::ptr session) override;

    virtual int32_t process(chat::grpc::GrpcRequest::ptr request,
                            chat::grpc::GrpcResult::ptr response,
                            chat::SocketStream::ptr stream) = 0;

    static std::string GetGrpcPath(const std::string& ns,
                    const std::string& service, const std::string& method);
};

class GrpcFunctionServlet : public GrpcServlet {
public:
    typedef std::shared_ptr<GrpcFunctionServlet> ptr;

    typedef std::function<int32_t(chat::grpc::GrpcRequest::ptr request,
                                  chat::grpc::GrpcResult::ptr response,
                                  chat::SocketStream::ptr stream)> callback;

    GrpcFunctionServlet(callback cb);
    int32_t process(chat::grpc::GrpcRequest::ptr request,
                    chat::grpc::GrpcResult::ptr response,
                    chat::SocketStream::ptr stream) override;

    static GrpcFunctionServlet::ptr Create(callback cb);
private:
    callback m_cb;
};

}
}

#endif