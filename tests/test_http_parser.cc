#include "../chat/chat.h"

chat::Logger::ptr g_logger = CHAT_LOG_ROOT();

const std::string test_req_data = 
    "GET / HTTP/1.1\r\n"
    "Host: www.bilibili.com\r\n"
    "Content-length: 10\r\n\r\n"
    "1234567890";

void test_req() {
    chat::http::HttpRequestParser parser;
    std::string tmp = test_req_data;
    size_t s = parser.execute(&tmp[0], tmp.size());
    CHAT_LOG_INFO(g_logger) << "execute rt=" << s
        << "has_error=" << parser.hasError()
        << " is_finished=" << parser.isFinished()
        << " total=" << tmp.size()
        << " content_length=" << parser.getContentLength();
    tmp.resize(tmp.size() - s);
    CHAT_LOG_INFO(g_logger) << parser.getData()->toString();
    CHAT_LOG_INFO(g_logger) << tmp;
}

const char test_response_data[] = "HTTP/1.1 200 OK\r\n"
        "Date: Tue, 04 Jun 2023 15:43:56 GMT\r\n"
        "Server: Apache\r\n"
        "Last-Modified: Tue, 12 Jan 2020 13:48:00 GMT\r\n"
        "ETag: \"51-47cf7e6ee8400\"\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Length: 81\r\n"
        "Cache-Control: max-age=86400\r\n"
        "Expires: Wed, 05 Jun 2023 15:43:56 GMT\r\n"
        "Connection: Close\r\n"
        "Content-Type: text/html\r\n\r\n"
        "<html>\r\n"
        "<meta http-equiv=\"refresh\" content=\"0;url=http://www.baidu.com/\">\r\n"
        "</html>\r\n";

void test_res() {
    chat::http::HttpResponseParser parser;
    std::string tmp = test_response_data;
    size_t s = parser.execute(&tmp[0], tmp.size(), true);
    CHAT_LOG_INFO(g_logger) << "execute rt=" << s
        << " has_error=" << parser.hasError()
        << " is_finished=" << parser.isFinished()
        << " total=" << tmp.size()
        << " content_length=" << parser.getContentLength()
        << " tmp[s]=" << tmp[s];

    tmp.resize(tmp.size() - s);

    CHAT_LOG_INFO(g_logger) << parser.getData()->toString();
    CHAT_LOG_INFO(g_logger) << tmp;
}

int main(int argc, char** argv) {
    test_req();
    CHAT_LOG_INFO(g_logger) << "--------";
    test_res();
    return 0;
}