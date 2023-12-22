#include "chat/socket.h"
#include "chat/log.h"
#include "chat/iomanager.h"

static chat::Logger::ptr g_logger = CHAT_LOG_ROOT();

void run() {
    chat::IPAddress::ptr addr = chat::Address::LookupAnyIPAddress("0.0.0.0:8020");
    chat::Socket::ptr sock = chat::Socket::CreateUDP(addr);
    if(sock->bind(addr)) {
        CHAT_LOG_INFO(g_logger) << "udp bind : " << *addr;
    } else {
        CHAT_LOG_ERROR(g_logger) << "udp bind : " << *addr << " fail";
        return;
    }
    while(true) {
        char buff[1024];
        chat::Address::ptr from(new chat::IPv4Address);
        int len = sock->recvFrom(buff, 1024, from);
        if(len > 0) {
            buff[len] = '\0';
            CHAT_LOG_INFO(g_logger) << "recv: " << buff << " from: " << *from;
            len = sock->sendTo(buff, len, from);
            if(len < 0) {
                CHAT_LOG_INFO(g_logger) << "send: " << buff << " to: " << *from
                    << " error=" << len;
            }
        }
    }
}

int main(int argc, char** argv) {
    chat::IOManager iom(1);
    iom.schedule(run);
    return 0;
}