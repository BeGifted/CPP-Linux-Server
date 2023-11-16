#include "../chat/chat.h"

chat::Logger::ptr g_logger = CHAT_LOG_ROOT();

void run() {
    auto addr = chat::Address::LookupAny("0.0.0.0:8033");
    //auto addr2 = chat::UnixAddress::ptr(new chat::UnixAddress("/tmp/unix_addr"));
    std::vector<chat::Address::ptr> addrs;
    addrs.push_back(addr);
    //addrs.push_back(addr2);

    chat::TcpServer::ptr tcp_server(new chat::TcpServer);
    std::vector<chat::Address::ptr> fails;
    while(!tcp_server->bind(addrs, fails)) {
        sleep(2);
    }
    tcp_server->start();
}

int main(int argc, char** argv) {
    chat::IOManager iom(2);
    iom.schedule(run);
    return 0;
}