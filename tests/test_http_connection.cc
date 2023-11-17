#include "../chat/chat.h"

chat::Logger::ptr g_logger = CHAT_LOG_ROOT();

void run() {
    chat::Address::ptr addr = chat::Address::LookupAnyIPAddress("baidu.com:80");
    if (!addr) {
        CHAT_LOG_INFO(g_logger) << "get addr err";
        return;
    }
    chat::Socket::ptr sock = chat::Socket::CreateTCP(addr);
    bool rt = sock->connect(addr);
    if (!rt) {
        CHAT_LOG_INFO(g_logger) << "connect" << addr->toString() << " fail";
        return;
    }
    chat::http::HttpConnection::ptr conn(new chat::http::HttpConnection(sock));
    chat::http::HttpRequest::ptr req(new chat::http::HttpRequest);
    req->setHeader("host", "www.baidu.com");
    CHAT_LOG_INFO(g_logger) << "req:" << std::endl << req->toString();

    conn->sendRequest(req);
    auto rsp = conn->recvResponse();

    if (!rsp) {
        CHAT_LOG_INFO(g_logger) << "recvResponse err";
        return;
    }
    CHAT_LOG_INFO(g_logger) << "rsp:" << std::endl << rsp->toString();
    
}

int main(int argc, char** argv) {
    chat::IOManager iom(2);
    iom.schedule(run);
    return 0;
}