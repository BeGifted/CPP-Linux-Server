#include "ws_server.h"
#include "chat/log.h"

namespace chat {
namespace http {

static chat::Logger::ptr g_logger = CHAT_LOG_NAME("system");

WSServer::WSServer(chat::IOManager* worker, chat::IOManager* io_worker, chat::IOManager* accept_worker)
    :TcpServer(worker, io_worker, accept_worker) {
    m_dispatch = std::make_shared<WSServletDispatch>();
    m_type = "websocket_server";
}

void WSServer::handleClient(Socket::ptr client) {
    CHAT_LOG_DEBUG(g_logger) << "handleClient " << *client;
    WSSession::ptr session = std::make_shared<WSSession>(client);
    do {
        HttpRequest::ptr header = session->handleShake();
        if(!header) {
            CHAT_LOG_DEBUG(g_logger) << "handleShake error";
            break;
        }
        WSServlet::ptr servlet = m_dispatch->getWSServlet(header->getPath());
        if(!servlet) {
            CHAT_LOG_DEBUG(g_logger) << "no match WSServlet";
            break;
        }
        int rt = servlet->onConnect(header, session);
        if(rt) {
            CHAT_LOG_DEBUG(g_logger) << "onConnect return " << rt;
            break;
        }
        while(true) {
            auto msg = session->recvMessage();
            if(!msg) {
                break;
            }
            rt = servlet->handle(header, msg, session);
            if(rt) {
                CHAT_LOG_DEBUG(g_logger) << "handle return " << rt;
                break;
            }
        }
        servlet->onClose(header, session);
    } while(0);
    session->close();
}

}
}
