#ifndef __CHAT_HTTP_CONNECTION_H__
#define __CHAT_HTTP_CONNECTION_H__

#include "chat/streams/socket_stream.h"
#include "http.h"
#include "chat/uri.h"
#include <vector>
#include <list>

namespace chat{
namespace http{

//HTTP响应结果
struct HttpResult {
    typedef std::shared_ptr<HttpResult> ptr;

    enum class Error {
        OK = 0,
        INVALID_URL = 1,
        INVALID_HOST = 2,
        CONNECT_FAIL = 3,
        SEND_CLOSE_BY_PEER = 4,
        SEND_SOCKET_ERROR = 5,
        TIMEOUT = 6,
        CREATE_SOCKET_ERROR = 7,
        POOL_GET_CONNECTION = 8,
        POOL_INVALID_CONNECTION = 9,
    };

    HttpResult(int _result, HttpResponse::ptr _response, const std::string& _error)
        :result(_result)
        ,response(_response)
        ,error(_error) {}

    int result;
    HttpResponse::ptr response;
    std::string error;
    std::string toString() const;
};


class HttpConnection: public SocketStream {
friend class HttpConnectionPool;
public:
    typedef std::shared_ptr<HttpConnection> ptr;

    //发送HTTP的GET请求
    static HttpResult::ptr DoGet(const std::string& url
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers = {}
                            , const std::string& body = "");
    static HttpResult::ptr DoGet(Uri::ptr uri
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers = {}
                            , const std::string& body = "");

    //发送HTTP的POST请求
    static HttpResult::ptr DoPost(const std::string& url
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers = {}
                            , const std::string& body = "");
    static HttpResult::ptr DoPost(Uri::ptr uri
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers = {}
                            , const std::string& body = "");

    //发送HTTP请求
    static HttpResult::ptr DoRequest(HttpMethod method
                            , const std::string& url
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers = {}
                            , const std::string& body = "");
    static HttpResult::ptr DoRequest(HttpMethod method
                            , Uri::ptr uri
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers = {}
                            , const std::string& body = "");
    static HttpResult::ptr DoRequest(HttpRequest::ptr req
                            , Uri::ptr uri
                            , uint64_t timeout_ms);
    
    static HttpResult::ptr DoRequest(HttpRequest::ptr req
                            , Address::ptr addr
                            , bool is_https
                            , uint64_t timeout_ms);

    static HttpResult::ptr DoRequest(HttpRequest::ptr req
                            , Socket::ptr sock
                            , uint64_t timeout_ms);

    HttpConnection(Socket::ptr sock, bool owner = true);

    HttpResponse::ptr recvResponse();
    int sendRequest(HttpRequest::ptr rsp);
private:
    uint64_t m_createTime = 0;
    uint64_t m_request = 0;
};

class HttpConnectionPool {
public:
    typedef std::shared_ptr<HttpConnectionPool> ptr;
    typedef Mutex MutexType;

    static HttpConnectionPool::ptr Create(const std::string& uri
                                   ,const std::string& vhost
                                   ,uint32_t max_size
                                   ,uint32_t max_alive_time
                                   ,uint32_t max_request);

    HttpConnectionPool(const std::string& host
                       ,const std::string& vhost
                       ,uint32_t port
                       ,bool is_https
                       ,uint32_t max_size
                       ,uint32_t max_alive_time
                       ,uint32_t max_request);

    HttpConnection::ptr getConnection(uint64_t& timeout_ms);

    HttpResult::ptr doGet(const std::string& url
                          , uint64_t timeout_ms
                          , const std::map<std::string, std::string>& headers = {}
                          , const std::string& body = "");
    HttpResult::ptr doGet(Uri::ptr uri
                           , uint64_t timeout_ms
                           , const std::map<std::string, std::string>& headers = {}
                           , const std::string& body = "");

    HttpResult::ptr doPost(const std::string& url
                           , uint64_t timeout_ms
                           , const std::map<std::string, std::string>& headers = {}
                           , const std::string& body = "");
    HttpResult::ptr doPost(Uri::ptr uri
                           , uint64_t timeout_ms
                           , const std::map<std::string, std::string>& headers = {}
                           , const std::string& body = "");

    HttpResult::ptr doRequest(HttpMethod method
                            , const std::string& url
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers = {}
                            , const std::string& body = "");
    HttpResult::ptr doRequest(HttpMethod method
                            , Uri::ptr uri
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers = {}
                            , const std::string& body = "");
    HttpResult::ptr doRequest(HttpRequest::ptr req
                            , uint64_t timeout_ms);

private:
    static void ReleasePtr(HttpConnection* ptr, HttpConnectionPool* pool);

private:
    std::string m_host;
    std::string m_vhost;
    uint32_t m_port;
    uint32_t m_maxSize;
    uint32_t m_maxAliveTime;
    uint32_t m_maxRequest;
    bool m_isHttps;
    std::string m_service;

    MutexType m_mutex;
    std::list<HttpConnection*> m_conns;
    std::atomic<int32_t> m_total = {0};
};

}
}



#endif