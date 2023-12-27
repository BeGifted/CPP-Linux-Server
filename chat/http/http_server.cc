#include "http_server.h"
#include "chat/log.h"

namespace chat {
namespace http {

static chat::Logger::ptr g_logger = CHAT_LOG_NAME("system");

HttpServer::HttpServer(bool keepalive
               ,chat::IOManager* worker
               ,chat::IOManager* io_worker
               ,chat::IOManager* accept_worker)
    :TcpServer(worker, io_worker, accept_worker)
    ,m_isKeepalive(keepalive) {
    m_dispatch = std::make_shared<ServletDispatch>();

    m_type = "http";
}

void HttpServer::setName(const std::string& v) {
    TcpServer::setName(v);
    m_dispatch->setDefault(std::make_shared<NotFoundServlet>(v));
}

void HttpServer::handleClient(Socket::ptr client) {
    CHAT_LOG_DEBUG(g_logger) << "handleClient " << client->toString();
    HttpSession::ptr session = std::make_shared<HttpSession>(client);
    do {
        auto req = session->recvRequest();
        if (!req) {
            CHAT_LOG_DEBUG(g_logger) << "recv http request fail, errno="
                << errno << ", errstr=" << strerror(errno)
                << ", cliet:" << client->toString() << ", keep_alive=" << m_isKeepalive;
            break;
        }

        HttpResponse::ptr rsp = std::make_shared<HttpResponse>(req->getVersion(), req->isClose() || !m_isKeepalive);
        rsp->setBody("hello");
        rsp->setHeader("Server", getName());
        {
            chat::SchedulerSwitcher sw(m_worker);
            m_dispatch->handle(req, rsp, session);
        }
        session->sendResponse(rsp);

        if (!m_isKeepalive || req->isClose()) {
            break;
        }
    } while(true);
    session->close();
}

}
}