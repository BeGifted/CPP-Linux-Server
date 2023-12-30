#include "chat/grpc/grpc_stream.h"
#include "chat/chat.h"
#include "test.pb.h"
#include "chat/log.h"

static chat::Logger::ptr g_logger = CHAT_LOG_NAME("system");

void run() {
    chat::Address::ptr addr = chat::Address::LookupAnyIPAddress("172.30.228.234:8099");
    chat::grpc::GrpcConnection::ptr conn(new chat::grpc::GrpcConnection);

    if(!conn->connect(addr, false)) {
        CHAT_LOG_INFO(g_logger) << "connect " << *addr << " fail" << std::endl;
        return;
    }
    conn->start();

    // sleep(1);
    // test::HelloRequest hr;
    // hr.set_id("hello");
    // hr.set_msg("world");
    // auto rsp = conn->request("/test.HelloService/Hello", hr, 100);
    // std::cout << rsp->toString() << std::endl;

    std::string  prefx = "aaaa";
    for(int i = 0; i < 4; ++i) {
        prefx = prefx + prefx;
    }

    for(int x = 0; x < 10; ++x) {
        chat::IOManager::GetThis()->schedule([conn, x, prefx](){
            int fail = 0;
            int error = 0;
            static int sn = 0;
            for(int i = 0; i < 10; ++i) {
                test::HelloRequest hr;
                auto tmp = chat::Atomic::addFetch(sn, 1);
                hr.set_id(prefx + "hello_" + std::to_string(tmp));
                hr.set_msg(prefx + "world_" + std::to_string(tmp));
                auto rsp = conn->request("/test.HelloService/Hello", hr, 100000);
                CHAT_LOG_INFO(g_logger) << rsp->toString() << std::endl;
                if(rsp->getResponse()) {
                    //std::cout << *rsp->getResponse() << std::endl;

                    auto hrp = rsp->getAsPB<test::HelloResponse>();
                    if(hrp) {
                        CHAT_LOG_INFO(g_logger) << "========" << rsp->getResult() << " - " << chat::PBToJsonString(hr);
                    } else { 
                        CHAT_LOG_INFO(g_logger) << "########" << rsp->getResult() << " - " << chat::PBToJsonString(hr);
                        ++error;
                    }
                } else {
                    ++fail;
                }
            }
            CHAT_LOG_INFO(g_logger) << "=======fail=" << fail << " error=" << error << " =========" << std::endl;
        });
    }
}

int main(int argc, char** argv) {
    chat::IOManager io(2);
    io.schedule(run);
    io.addTimer(10000, [](){}, true);
    io.stop();
    return 0;
}