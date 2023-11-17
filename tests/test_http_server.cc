#include "../chat/chat.h"

chat::Logger::ptr g_logger = CHAT_LOG_ROOT();

void run() {
    chat::http::HttpServer::ptr server(new chat::http::HttpServer);
    chat::Address::ptr addr = chat::Address::LookupAnyIPAddress("0.0.0.0:8020");
    while (!server->bind(addr))
    {
        sleep(2);
    }
    auto sd = server->getServletDispatch();
    sd->addServlet("/chat/xx", [](chat::http::HttpRequest::ptr req
        ,chat::http::HttpResponse::ptr rsp
        ,chat::http::HttpSession::ptr session){
        rsp->setBody(req->toString());
        return 0;
    });

    sd->addGlobServlet("/chat/*", [](chat::http::HttpRequest::ptr req
        ,chat::http::HttpResponse::ptr rsp
        ,chat::http::HttpSession::ptr session){
        rsp->setBody("Glob: " + req->toString());
        return 0;
    });

    server->start();
    
}

int main(int argc, char** argv) {
    chat::IOManager iom(2);
    iom.schedule(run);
    return 0;
}