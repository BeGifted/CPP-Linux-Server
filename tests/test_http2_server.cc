#include "chat/http2/http2_server.h"
#include "chat/http/http_server.h"
#include "chat/log.h"

static chat::Logger::ptr g_logger = CHAT_LOG_NAME("system");

void run() {
    chat::http2::Http2Server::ptr server(new chat::http2::Http2Server);
    chat::Address::ptr addr = chat::Address::LookupAnyIPAddress("0.0.0.0:8099");
    //server->bind(addr, true);
    //server->loadCertificates("rootCA.pem", "rootCA.key");

    auto sd = server->getServletDispatch();
    sd->addServlet("/hello", 
    [](chat::http::HttpRequest::ptr request
                   , chat::http::HttpResponse::ptr response
                   , chat::SocketStream::ptr session) {
        
        CHAT_LOG_INFO(g_logger) << *request;

        response->setBody("hello http2");
        return 0;
    });


    server->bind(addr, false);
    //server->bind(addr, true);
    //server->loadCertificates("server.crt", "server.key");
    server->start();
}

void run2() {
    chat::http::HttpServer::ptr server(new chat::http::HttpServer);
    chat::Address::ptr addr = chat::Address::LookupAnyIPAddress("0.0.0.0:8090");
    server->bind(addr, true);
    server->loadCertificates("server.crt", "server.key");
    server->start();
}

int main(int argc, char** argv) {
    chat::IOManager iom(2);
    iom.schedule(run);
    // iom.schedule(run2);
    iom.stop();
    return 0;
}