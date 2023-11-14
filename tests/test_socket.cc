#include "../chat/chat.h"

static chat::Logger::ptr g_logger = CHAT_LOG_ROOT();

void test_socket() {
    chat::IPAddress::ptr addr = chat::Address::LookupAnyIPAddress("www.baidu.com");
    if (addr) {
        CHAT_LOG_INFO(g_logger) << "get address: " << addr->toString();
    } else {
        CHAT_LOG_ERROR(g_logger) << "get address fail";
        return;
    }

    chat::Socket::ptr sock = chat::Socket::CreateTCP(addr);
    addr->setPort(80);
    CHAT_LOG_INFO(g_logger) << "addr=" << addr->toString();
    if (!sock->connect(addr)) {
        CHAT_LOG_ERROR(g_logger) << "connect " << addr->toString() << " fail";
        return;
    } else {
        CHAT_LOG_INFO(g_logger) << "connect " << addr->toString() << " connected";
    }

    const char buff[] = "GET / HTTP/1.0\r\n\r\n";
    int rt = sock->send(buff, sizeof(buff));
    if (rt <= 0) {
        CHAT_LOG_INFO(g_logger) << "send fail rt=" << rt;
        return;
    }

    std::string buffs;
    buffs.resize(4096);
    rt = sock->recv(&buffs[0], buffs.size());

    if(rt <= 0) {
        CHAT_LOG_INFO(g_logger) << "recv fail rt=" << rt;
        return;
    }

    buffs.resize(rt);
    CHAT_LOG_INFO(g_logger) << buffs;
}

void test2() {
    chat::IPAddress::ptr addr = chat::Address::LookupAnyIPAddress("www.baidu.com:80");
    if(addr) {
        CHAT_LOG_INFO(g_logger) << "get address: " << addr->toString();
    } else {
        CHAT_LOG_ERROR(g_logger) << "get address fail";
        return;
    }

    chat::Socket::ptr sock = chat::Socket::CreateTCP(addr);
    if(!sock->connect(addr)) {
        CHAT_LOG_ERROR(g_logger) << "connect " << addr->toString() << " fail";
        return;
    } else {
        CHAT_LOG_INFO(g_logger) << "connect " << addr->toString() << " connected";
    }

    uint64_t ts = chat::GetCurrentUs();
    for(size_t i = 0; i < 10000000000ul; ++i) {
        if(int err = sock->getError()) {
            CHAT_LOG_INFO(g_logger) << "err=" << err << " errstr=" << strerror(err);
            break;
        }

        //struct tcp_info tcp_info;
        //if(!sock->getOption(IPPROTO_TCP, TCP_INFO, tcp_info)) {
        //    CHAT_LOG_INFO(g_logger) << "err";
        //    break;
        //}
        //if(tcp_info.tcpi_state != TCP_ESTABLISHED) {
        //    CHAT_LOG_INFO(g_logger)
        //            << " state=" << (int)tcp_info.tcpi_state;
        //    break;
        //}
        static int batch = 10000000;
        if(i && (i % batch) == 0) {
            uint64_t ts2 = chat::GetCurrentUs();
            CHAT_LOG_INFO(g_logger) << "i=" << i << " used: " << ((ts2 - ts) * 1.0 / batch) << " us";
            ts = ts2;
        }
    }
}

int main(int argc, char** argv) {
    chat::IOManager iom;
    iom.schedule(&test_socket);

    return 0;
}