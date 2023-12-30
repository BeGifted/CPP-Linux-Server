#ifndef __CHAT_HTTP2_SERVER_H__
#define __CHAT_HTTP2_SERVER_H__

#include "http2_stream.h"
#include "chat/tcp_server.h"
#include "chat/http/servlet.h"

namespace chat {
namespace http2 {

class Http2Server : public TcpServer {
public:
    typedef std::shared_ptr<Http2Server> ptr;
    Http2Server(chat::IOManager* worker = chat::IOManager::GetThis()
                ,chat::IOManager* io_worker = chat::IOManager::GetThis()
                ,chat::IOManager* accept_worker = chat::IOManager::GetThis());

    http::ServletDispatch::ptr getServletDispatch() const { return m_dispatch;}
    void setServletDispatch(http::ServletDispatch::ptr v) { m_dispatch = v;}

    virtual void setName(const std::string& v) override;
protected:
    virtual void handleClient(Socket::ptr client) override;
private:
    http::ServletDispatch::ptr m_dispatch;
};

}
}

#endif