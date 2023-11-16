#include "../chat/chat.h"

chat::Logger::ptr g_logger = CHAT_LOG_ROOT();

void test_req() {
    chat::http::HttpRequest::ptr req(new chat::http::HttpRequest);
    req->setHeader("host", "www.baidu.com");
    req->setBody("shit baidu");

    req->dump(std::cout) << std::endl;
}

void test_res() {
    chat::http::HttpResponse::ptr res(new chat::http::HttpResponse);
    res->setHeader("X-X", "chat");
    res->setStatus((chat::http::HttpStatus)400);
    res->setBody("shit baidu");
    res->setClose(false);

    res->dump(std::cout) << std::endl;
}

int main(int argc, char** argv) {
    test_req();
    test_res();
    return 0;
}