#include "http2_server.h"
#include "chat/log.h"

namespace chat {
namespace http2 {

static chat::Logger::ptr g_logger = CHAT_LOG_NAME("system");

Http2Server::Http2Server(const std::string& type
                        ,chat::IOManager* worker
                        ,chat::IOManager* io_worker
                        ,chat::IOManager* accept_worker)
    :TcpServer(worker, io_worker, accept_worker) {
    m_type = type;
    m_dispatch = std::make_shared<http::ServletDispatch>();
}

void Http2Server::setName(const std::string& v) {
    TcpServer::setName(v);
    m_dispatch->setDefault(std::make_shared<http::NotFoundServlet>(v));
}

void Http2Server::handleClient(Socket::ptr client) {
    CHAT_LOG_DEBUG(g_logger) << "handleClient " << *client;
    chat::http2::Http2Session::ptr session = std::make_shared<chat::http2::Http2Session>(client, this);
    if(!session->handleShakeServer()) {
        CHAT_LOG_WARN(g_logger) << "http2 session handleShake fail, " << session->getRemoteAddressString();
        session->close();
        return;
    }
    session->setWorker(m_worker);
    session->start();
}

}
}