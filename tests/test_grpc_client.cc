#include "chat/grpc/grpc_stream.h"
#include "chat/chat.h"
#include "test.pb.h"

void run() {
    chat::Address::ptr addr = chat::Address::LookupAnyIPAddress("127.0.0.1:8099");
    chat::grpc::GrpcConnection::ptr conn(new chat::grpc::GrpcConnection);

    if(!conn->connect(addr, false)) {
        std::cout << "connect " << *addr << " fail" << std::endl;
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
    for(int i = 0; i < 23; ++i) {
        prefx = prefx + prefx;
    }

    for(int x = 0; x < 1; ++x) {
        chat::IOManager::GetThis()->schedule([conn, x, prefx](){
            int fail = 0;
            int error = 0;
            static int sn = 0;
            for(int i = 0; i < 1; ++i) {
                test::HelloRequest hr;
                auto tmp = chat::Atomic::addFetch(sn, 1);
                hr.set_id(prefx + "hello_" + std::to_string(tmp));
                hr.set_msg(prefx + "world_" + std::to_string(tmp));
                auto rsp = conn->request("/test.HelloService/Hello", hr, 100000);
                std::cout << rsp->toString() << std::endl;
                if(rsp->getResponse()) {
                    //std::cout << *rsp->getResponse() << std::endl;

                    auto hrp = rsp->getAsPB<test::HelloResponse>();
                    if(hrp) {
                        //std::cout << chat::PBToJsonString(*hrp) << std::endl;
                        std::cout << "========" << rsp->getResult() << std::endl;
                    } else {
                        std::cout << "########" << rsp->getResult() << std::endl;
                        ++error;
                    }
                } else {
                    ++fail;
                }
            }
            std::cout << "=======fail=" << fail << " error=" << error << " =========" << std::endl;
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