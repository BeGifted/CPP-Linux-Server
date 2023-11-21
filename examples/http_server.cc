#include "chat/http/http_server.h"
#include "chat/log.h"

chat::Logger::ptr g_logger = CHAT_LOG_ROOT();
// chat::IOManager::ptr worker;

void run() {
    g_logger->setLevel(chat::LogLevel::INFO);
    chat::Address::ptr addr = chat::Address::LookupAnyIPAddress("0.0.0.0:8020");
    if (!addr) {
        CHAT_LOG_ERROR(g_logger) << "get address error";
        return;
    }

    //chat::http::HttpServer::ptr http_server(new chat::http::HttpServer(true, worker.get()));
    chat::http::HttpServer::ptr http_server(new chat::http::HttpServer(true));
    // bool ssl = false;
    while (!http_server->bind(addr)) {
        CHAT_LOG_ERROR(g_logger) << "bind " << *addr << " fail";
        sleep(1);
    }

    // if(ssl) {
    //     //http_server->loadCertificates("/home/apps/soft/chat/keys/server.crt", "/home/apps/soft/chat/keys/server.key");
    // }

    http_server->start();
}

int main(int argc, char** argv) {
    chat::IOManager iom(1);
    // worker.reset(new chat::IOManager(4, false));
    iom.schedule(run);
    return 0;
}