#include "chat/tcp_server.h"
#include "chat/log.h"
#include "chat/iomanager.h"
#include "chat/bytearray.h"
#include "chat/address.h"
#include "chat/worker.h"

static chat::Logger::ptr g_logger = CHAT_LOG_ROOT();

class EchoServer : public chat::TcpServer {
public:
    EchoServer(int type);
    void handleClient(chat::Socket::ptr client);

private:
    int m_type = 0;
};

EchoServer::EchoServer(int type)
    :m_type(type) {
}

void EchoServer::handleClient(chat::Socket::ptr client) {
    CHAT_LOG_INFO(g_logger) << "handleClient " << client->toString();   
    chat::ByteArray::ptr ba(new chat::ByteArray);
    while (true) {
        ba->clear();
        std::vector<iovec> iovs;
        ba->getWriteBuffers(iovs, 1024);

        int rt = client->recv(&iovs[0], iovs.size());
        if (rt == 0) {
            CHAT_LOG_INFO(g_logger) << "client close: " << client->toString();
            break;
        } else if (rt < 0) {
            CHAT_LOG_INFO(g_logger) << "client error rt=" << rt
                << " errno=" << errno << " errstr=" << strerror(errno);
            break;
        }
        ba->setPosition(ba->getPosition() + rt);  //为了修改size
        ba->setPosition(0);
        if (m_type == 1) { //text
            std::cout << ba->toString();
        } else {
            std::cout << ba->toHexString();
        }
        std::cout.flush();
    }
}

int type = 1;

void test() {
    CHAT_LOG_INFO(g_logger) << "=========== test begin";
    chat::TimeCalc tc;
    chat::TimedWorkerGroup::ptr wg = chat::TimedWorkerGroup::Create(5, 1000);
    for(size_t i = 0; i < 10; ++i) {
        wg->schedule([i](){
            sleep(i);
            CHAT_LOG_INFO(g_logger) << "=========== " << i;
        });
    }
    wg->waitAll();
    CHAT_LOG_INFO(g_logger) << "=========== " << tc.elapse() << " over";
}


void run() {
    CHAT_LOG_INFO(g_logger) << "server type=" << type;
    EchoServer::ptr es(new EchoServer(type));
    auto addr = chat::Address::LookupAny("0.0.0.0:8020");
    while (!es->bind(addr)) {
        sleep(2);
    }
    es->start();
    chat::IOManager::GetThis()->schedule(test);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        CHAT_LOG_INFO(g_logger) << "used as[" << argv[0] << " -t] or [" << argv[0] << " -b]";
        return 0;
    }

    if(!strcmp(argv[1], "-b")) {
        type = 2;
    }

    chat::IOManager iom(2);
    iom.schedule(run);
    return 0;
}