#include "../chat/chat.h"

chat::Logger::ptr g_logger = CHAT_LOG_ROOT();

void test_pool() {
    chat::http::HttpConnectionPool::ptr pool(new chat::http::HttpConnectionPool(
                "www.baidu.com", "", 80, false, 10, 1000 * 30, 5));

    chat::IOManager::GetThis()->addTimer(1000, [pool](){
            auto r = pool->doGet("/", 300);
            CHAT_LOG_INFO(g_logger) << r->toString();
    }, true);
}

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
    
    // std::ofstream ofs("rsp.dat");
    // ofs << rsp->toString();

    CHAT_LOG_INFO(g_logger) << "=========================";

    auto r = chat::http::HttpConnection::DoGet("http://www.baidu.com", 300);
    CHAT_LOG_INFO(g_logger) << "result=" << r->result
        << " error=" << r->error
        << " rsp=" << (r->response ? r->response->toString() : "");

    CHAT_LOG_INFO(g_logger) << "=========================";
    test_pool();
}

void test_https() {
    auto r = chat::http::HttpConnection::DoGet("www.baidu.com", 300, {
                        {"Accept-Encoding", "gzip, deflate, br"},
                        // {"Connection", "keep-alive"}
            });
    CHAT_LOG_INFO(g_logger) << "result=" << r->result
        << " error=" << r->error
        << " rsp=" << (r->response ? r->response->toString() : "");

    chat::http::HttpConnectionPool::ptr pool(new chat::http::HttpConnectionPool(
                "www.baidu.com", "", 443, true, 10, 1000 * 30, 5));

    chat::IOManager::GetThis()->addTimer(1000, [pool](){
            auto r = pool->doGet("/", 300, {
                        {"Accept-Encoding", "gzip, deflate, br"}
                    });
            CHAT_LOG_INFO(g_logger) << r->toString();
    }, true);
}

int main(int argc, char** argv) {
    chat::IOManager iom(2);
    iom.schedule(test_https);
    return 0;
}