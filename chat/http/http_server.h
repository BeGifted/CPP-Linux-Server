#ifndef __CHAT_HTTP_SERVER_H__
#define __CHAT_HTTP_SERVER_H__

#include "chat/tcp_server.h"
#include "http_session.h"
#include "servlet.h"

namespace chat{
namespace http{

class HttpServer: public TcpServer {
public:
    typedef std::shared_ptr<HttpServer> ptr;

    HttpServer(bool keepalive = false
               ,chat::IOManager* worker = chat::IOManager::GetThis()
               ,chat::IOManager* io_worker = chat::IOManager::GetThis()
               ,chat::IOManager* accept_worker = chat::IOManager::GetThis());

    ServletDispatch::ptr getServletDispatch() const { return m_dispatch;}
    void setServletDispatch(ServletDispatch::ptr v) { m_dispatch = v;}

    virtual void setName(const std::string& v) override;

protected:
    virtual void handleClient(Socket::ptr client) override;

private:
    //是否支持长连接
    bool m_isKeepalive;
    //Servlet分发器
    ServletDispatch::ptr m_dispatch;
};

}


}



#endif