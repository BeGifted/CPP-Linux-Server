#include "chat/grpc/grpc_stream.h"
#include "tests/test.pb.h"
#include "chat/log.h"
#include "chat/chat.h"
#include "chat/grpc/grpc_connection.h"

static chat::Logger::ptr g_logger = CHAT_LOG_NAME("system");

void testUnary(chat::grpc::GrpcConnection::ptr conn) {
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
                auto hr = std::make_shared<test::HelloRequest>();
                auto tmp = chat::Atomic::addFetch(sn, 1);
                hr->set_id(prefx + "hello_" + std::to_string(tmp));
                hr->set_msg(prefx + "world_" + std::to_string(tmp));
                auto rsp = conn->request("/test.HelloService/Hello", hr, 100000);
                CHAT_LOG_INFO(g_logger) << rsp->toString() << std::endl;
                if(rsp->getResponse()) {
                    //std::cout << *rsp->getResponse() << std::endl;

                    auto hrp = rsp->getAsPB<test::HelloResponse>();
                    if(hrp) {
                        //std::cout << chat::PBToJsonString(*hrp) << std::endl;
                        CHAT_LOG_INFO(g_logger) << "========" << rsp->getResult() << " - " << chat::PBToJsonString(*hr);
                    } else { 
                        CHAT_LOG_INFO(g_logger) << "########" << rsp->getResult() << " - " << chat::PBToJsonString(*hr);
                        ++error;
                    }
                    //CHAT_LOG_INFO(g_logger) << *rsp->getResponse();
                } else {
                    ++fail;
                }
            }
            CHAT_LOG_INFO(g_logger) << "=======fail=" << fail << " error=" << error << " =========" << std::endl;
        });
    }
}

void testStreamClient(chat::grpc::GrpcConnection::ptr conn) {
    while(true) {
        for(int x = 0; x < 100; ++x) {
            auto stm = conn->openGrpcStream("/test.HelloService/HelloStreamA");
            //sleep(1);
            for(int i = 0; i < 10; ++i) {
                auto hr = std::make_shared<test::HelloRequest>();
                hr->set_id("hello_stream_a");
                hr->set_msg("world_stream_a");

                stm->sendMessage(hr);
            }
            stm->sendData("", true); // end stream
            auto rsp = stm->recvMessage<test::HelloResponse>();
            CHAT_LOG_INFO(g_logger) << "===== HelloStreamA - " << rsp << " - " << (rsp ? chat::PBToJsonString(*rsp) : "(null)");
            //sleep(1);
        }
        sleep(12);
    }
}

void testStreamClient2(chat::grpc::GrpcConnection::ptr conn) {
    while(true) {
        for(int x = 0; x < 100; ++x) {
            auto stm = conn->openGrpcClientStream<test::HelloRequest, test::HelloResponse>("/test.HelloService/HelloStreamA");
            //sleep(1);
            for(int i = 0; i < 10; ++i) {
                auto hr = std::make_shared<test::HelloRequest>();
                hr->set_id("hello_stream_a");
                hr->set_msg("world_stream_a");

                if(stm->send(hr) <= 0) {
                    CHAT_LOG_INFO(g_logger) << "testStreamClient2 send fail";
                    break;
                }
            }
            auto rsp = stm->closeAndRecv();
            CHAT_LOG_INFO(g_logger) << "===== testStreamClient2 HelloStreamA - " << rsp << " - " << (rsp ? chat::PBToJsonString(*rsp) : "(null)");
            //sleep(1);
        }
        sleep(12);
    }
}

void testStreamServer(chat::grpc::GrpcConnection::ptr conn) {
    while(true) {
        for(int x = 0; x < 100; ++x) {
            auto stm = conn->openGrpcStream("/test.HelloService/HelloStreamB");
            auto hr = std::make_shared<test::HelloRequest>();
            hr->set_id("hello_stream_b");
            hr->set_msg("world_stream_b");
            stm->sendMessage(hr, true);
            while(true) {
                auto rsp = stm->recvMessage<test::HelloResponse>();
                if(rsp) {
                    CHAT_LOG_INFO(g_logger) << "===== HelloStreamB - " << rsp << " - " << (rsp ? chat::PBToJsonString(*rsp) : "(null)");
                } else {
                    CHAT_LOG_INFO(g_logger) << "===== HelloStreamB Out";
                    break;
                }
            }
            //sleep(1);
        }
        sleep(12);
    }
}

void testStreamServer2(chat::grpc::GrpcConnection::ptr conn) {
    while(true) {
        for(int x = 0; x < 100; ++x) {
            auto hr = std::make_shared<test::HelloRequest>();
            hr->set_id("hello_stream_b");
            hr->set_msg("world_stream_b");
            auto stm = conn->openGrpcServerStream<test::HelloRequest, test::HelloResponse>("/test.HelloService/HelloStreamB", hr);
            while(true) {
                auto rsp = stm->recv();
                if(rsp) {
                    CHAT_LOG_INFO(g_logger) << "testStreamServer2 ===== HelloStreamB - " << rsp << " - " << (rsp ? chat::PBToJsonString(*rsp) : "(null)");
                } else {
                    CHAT_LOG_INFO(g_logger) << "testStreamServer2 ===== HelloStreamB Out";
                    break;
                }
            }
            //sleep(1);
        }
        sleep(12);
    }
}

void testStreamBidi(chat::grpc::GrpcConnection::ptr conn) {
    while(true) {
        for(int x = 0; x < 100; ++x) {
            auto stm = conn->openGrpcStream("/test.HelloService/HelloStreamC");
            auto wg = chat::WorkerGroup::Create(1);
            wg->schedule([stm](){
                for(int i = 0; i< 10; ++i) {
                    auto hr = std::make_shared<test::HelloRequest>();
                    hr->set_id("hello_stream_c");
                    hr->set_msg("world_stream_c");
                    stm->sendMessage(hr);
                }
                stm->sendData("", true);
            });
            while(true) {
                auto rsp = stm->recvMessage<test::HelloResponse>();
                if(rsp) {
                    CHAT_LOG_INFO(g_logger) << "===== HelloStreamC - " << rsp << " - " << (rsp ? chat::PBToJsonString(*rsp) : "(null)");
                } else {
                    CHAT_LOG_INFO(g_logger) << "===== HelloStreamC Out";
                    break;
                }
            }
            wg->waitAll();
            sleep(5);
            CHAT_LOG_INFO(g_logger) << "-------------------------------";
        }
        sleep(12);
    }
}

void testStreamBidi2(chat::grpc::GrpcConnection::ptr conn) {
    while(true) {
        for(int x = 0; x < 100; ++x) {
            auto stm = conn->openGrpcBidirectionStream<test::HelloRequest, test::HelloResponse>("/test.HelloService/HelloStreamC");
            auto wg = chat::WorkerGroup::Create(1);
            wg->schedule([stm](){
                for(int i = 0; i< 10; ++i) {
                    auto hr = std::make_shared<test::HelloRequest>();
                    hr->set_id("hello_stream_c");
                    hr->set_msg("world_stream_c");
                    if(stm->send(hr) <= 0) {
                        CHAT_LOG_INFO(g_logger) << "testStreamBidi2 send fail";
                        break;
                    }
                }
                sleep(1);
                stm->close();
            });
            while(true) {
                auto rsp = stm->recv();
                if(rsp) {
                    CHAT_LOG_INFO(g_logger) << "testStreamBidi2 ===== HelloStreamC - " << rsp << " - " << (rsp ? chat::PBToJsonString(*rsp) : "(null)");
                } else {
                    CHAT_LOG_INFO(g_logger) << "testStreamBidi2 ===== HelloStreamC Out";
                    break;
                }
            }
            wg->waitAll();
            sleep(5);
            CHAT_LOG_INFO(g_logger) << "-------------------------------";
        }
        sleep(12);
    }
}

void run() {
    chat::Address::ptr addr = chat::Address::LookupAnyIPAddress("172.30.228.234:8099");
    chat::grpc::GrpcConnection::ptr conn(new chat::grpc::GrpcConnection);

    if(!conn->connect(addr, false)) {
        CHAT_LOG_INFO(g_logger) << "connect " << *addr << " fail" << std::endl;
        return;
    }
    conn->start();
    //sleep(1);
    chat::IOManager::GetThis()->addTimer(10000, [conn](){
            CHAT_LOG_INFO(g_logger) << "------ status ------";
            CHAT_LOG_INFO(g_logger) << conn->isConnected();
            CHAT_LOG_INFO(g_logger) << conn->recving;
            CHAT_LOG_INFO(g_logger) << "------ status ------";
    }, true);
    //sleep(5);
    chat::IOManager::GetThis()->schedule(std::bind(testUnary, conn));
    //for(int i = 0; i < 1000; ++i) {
    //    //chat::IOManager::GetThis()->schedule(std::bind(testStreamClient, conn));
    //    //chat::IOManager::GetThis()->schedule(std::bind(testStreamServer, conn));
    //    chat::IOManager::GetThis()->schedule(std::bind(testStreamBidi, conn));
    //    sleep(10);
    //}

    //testStreamBidi(conn);
    //testStreamClient2(conn);
    //testStreamServer2(conn);
    testStreamBidi2(conn);
}

int main(int argc, char** argv) {
    chat::IOManager io(2);
    io.schedule(run);
    io.addTimer(10000, [](){}, true);
    io.stop();
    return 0;
}