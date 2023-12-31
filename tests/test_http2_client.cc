#include "chat/chat.h"
#include "chat/http2/http2_connection.h"

std::string bigstr;

void test() {
    bigstr = "aaaaaaaaaa";
    for(int i = 0; i < 1; ++i) {
        bigstr = bigstr + bigstr;
    }
    chat::Address::ptr addr = chat::Address::LookupAnyIPAddress("172.30.228.234:8099");
    chat::http2::Http2Connection::ptr stream(new chat::http2::Http2Connection());
    //if(!stream->connect(addr, true)) {
    if(!stream->connect(addr, false)) {
        std::cout << "connect " << *addr << " fail";
    }
    //if(stream->handleShakeClient()) {
    //    std::cout << "handleShakeClient ok," << stream->isConnected() << std::endl;
    //} else {
    //    std::cout << "handleShakeClient fail" << std::endl;
    //    return;
    //}
    stream->start();
    //sleep(1);

    for(int x = 0; x < 30; ++x) {
        chat::IOManager::GetThis()->schedule([stream, x](){
            for(int i = 0; i < 10; ++i) {
                chat::http::HttpRequest::ptr req(new chat::http::HttpRequest);
                //req->setHeader(":path", "/");
                //req->setHeader(":method", "GET");
                //req->setHeader(":scheme", "http");
                //req->setHeader(":host", "127.0.0.1");

                req->setHeader(":method", "GET");
                req->setHeader(":scheme", "http");
                // req->setHeader(":path", "/_/config?abc=111#cde");
                // req->setHeader(":authority", "127.0.0.1:8090");
                req->setHeader("content-type", "text/html");
                //req->setHeader("content-encoding", "gzip");
                req->setHeader("user-agent", "grpc-go/1.37.0");
                req->setHeader("hello", "world");
                req->setHeader("id", std::to_string(x) + "_" + std::to_string(i));

                auto body = bigstr + "_hello";
                //auto zs = chat::ZlibStream::CreateGzip(true);
                //zs->write(body.c_str(), body.size());
                //zs->flush();
                //zs->getResult().swap(body);
                req->setBody(body);

                auto rt = stream->request(req, 100000);
                //std::cout << "----" << rt->result << std::endl;
                //std::cout << "bigstr.length=" << bigstr.length() << std::endl;
                std::cout << "----" << rt->toString() << std::endl;
                //sleep(1);
            }
        });
    }
}

int main(int argc, char** argv) {
    chat::IOManager iom;
    iom.schedule(test);
    iom.addTimer(1000, [](){}, true);
    iom.stop();
    return 0;
}