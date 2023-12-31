#include "chat/grpc/grpc_server.h"
#include "chat/grpc/grpc_servlet.h"
#include "tests/test.pb.h"
#include "chat/log.h"

static chat::Logger::ptr g_logger = CHAT_LOG_NAME("test");

class HelloServiceHello
    : public chat::grpc::GrpcUnaryServlet<test::HelloRequest, test::HelloResponse> {
public:
    HelloServiceHello()
        :Base("HelloServiceHello") {
    }
    int32_t handle(ReqPtr req, RspPtr rsp) override {
        CHAT_LOG_INFO(g_logger) << "HelloServiceHello req= " << chat::PBToJsonString(*req)
            << " request=" << getRequest()
            << " response=" << getResponse()
            << " session=" << getSession()
            << " stream=" << getStream();
        rsp->set_id("id HelloServiceHello");
        rsp->set_msg("msg HelloServiceHello");
        return 0;
    }

    GrpcServlet::ptr clone() override {
        return std::make_shared<HelloServiceHello>(*this);
    }
};

class HelloServiceHelloStreamA
    : public chat::grpc::GrpcStreamClientServlet<test::HelloRequest, test::HelloResponse> {
public:
    HelloServiceHelloStreamA()
        :Base("HelloServiceHelloStreamA") {
    }

    int32_t handle(ServerStream::ptr stream, RspPtr rsp) {
        CHAT_LOG_INFO(g_logger) << "HelloServiceHelloStreamA stream=" << stream;
        while(true) {
            auto req = stream->recv();
            if(req) {
                CHAT_LOG_INFO(g_logger) << "HelloServiceHelloStreamA req=" << chat::PBToJsonString(*req);
            } else {
                CHAT_LOG_INFO(g_logger) << "HelloServiceHelloStreamA out";
                break;
            }
        }
        rsp->set_id("id HelloServiceHelloStreamA");
        rsp->set_msg("msg HelloServiceHelloStreamA");
        return 0;
    }

    GrpcServlet::ptr clone() override {
        return std::make_shared<HelloServiceHelloStreamA>(*this);
    }
};

class HelloServiceHelloStreamB
    : public chat::grpc::GrpcStreamServerServlet<test::HelloRequest, test::HelloResponse> {
public:
    int32_t handle(ServerStream::ptr stream, ReqPtr req) {
        CHAT_LOG_INFO(g_logger) << "HelloServiceHelloStreamB stream=" << stream;
        for(int i = 0; i < 10; ++i) {
            auto rsp = std::make_shared<test::HelloResponse>();
            rsp->set_id("id HelloServiceHelloStreamB");
            rsp->set_msg("msg HelloServiceHelloStreamB");
            if(stream->send(rsp) <= 0) {
                CHAT_LOG_INFO(g_logger) << "send rsp error";
                break;
            }
        }
        CHAT_LOG_INFO(g_logger) << "HelloServiceHelloStreamB out";
        return 0;
    }

    GRPC_SERVLET_INIT(HelloServiceHelloStreamB);
};

class HelloServiceHelloStreamC
    : public chat::grpc::GrpcStreamBidirectionServlet<test::HelloRequest, test::HelloResponse> {
public:
    int32_t handle(ServerStream::ptr stream) {
        CHAT_LOG_INFO(g_logger) << "HelloServiceHelloStreamC stream=" << stream;
        for(int i = 0; i < 10; ++i) {
            auto rsp = std::make_shared<test::HelloResponse>();
            rsp->set_id("id HelloServiceHelloStreamC");
            rsp->set_msg("msg HelloServiceHelloStreamC");
            if(stream->send(rsp) <= 0) {
                CHAT_LOG_INFO(g_logger) << "send rsp error";
                break;
            }
        }
        while(true) {
            auto req = stream->recv();
            if(req) {
                CHAT_LOG_INFO(g_logger) << "HelloServiceHelloStreamC req=" << chat::PBToJsonString(*req);
            } else {
                CHAT_LOG_INFO(g_logger) << "HelloServiceHelloStreamC recv out";
                break;
            }
        }
        CHAT_LOG_INFO(g_logger) << "HelloServiceHelloStreamC out";
        return 0;
    }

    GRPC_SERVLET_INIT(HelloServiceHelloStreamC);
};


void run() {
    chat::grpc::GrpcServer::ptr server(new chat::grpc::GrpcServer);
    chat::Address::ptr addr = chat::Address::LookupAnyIPAddress("0.0.0.0:8099");

    //std::shared_ptr<HelloServiceHello> hsh = std::make_shared<HelloServiceHello>();
    //std::shared_ptr<HelloServiceHello> hsh2 = std::make_shared<HelloServiceHello>(*hsh);

    server->addGrpcServlet(chat::grpc::GrpcServlet::GetGrpcPath("test", "HelloService", "Hello"),
                           std::make_shared<HelloServiceHello>()
                           );
    server->addGrpcServlet(chat::grpc::GrpcServlet::GetGrpcPath("test", "HelloService", "HelloStreamA"),
                           std::make_shared<HelloServiceHelloStreamA>()
            );
    server->addGrpcServlet(chat::grpc::GrpcServlet::GetGrpcPath("test", "HelloService", "HelloStreamB"),
                           std::make_shared<HelloServiceHelloStreamB>());

    server->addGrpcServlet(chat::grpc::GrpcServlet::GetGrpcPath("test", "HelloService", "HelloStreamC"),
                           std::make_shared<HelloServiceHelloStreamC>());
    server->bind(addr, false);
    server->start();
}

int main(int argc, char** argv) {
    chat::IOManager iom(2);
    iom.schedule(run);
    iom.stop();
    return 0;
}