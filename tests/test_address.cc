#include "../chat/chat.h"

chat::Logger::ptr g_logger = CHAT_LOG_ROOT();

void test() {
    std::vector<chat::Address::ptr> addrs;

    CHAT_LOG_INFO(g_logger) << "lookup begin";
    bool v = chat::Address::Lookup(addrs, "www.baidu.com", AF_INET, SOCK_STREAM);
    CHAT_LOG_INFO(g_logger) << "lookup end";
    if (!v) {
        CHAT_LOG_ERROR(g_logger) << "lookup fail";
        return;
    }
    for (size_t i = 0; i < addrs.size(); i++) {
        CHAT_LOG_INFO(g_logger) << i << "-" << addrs[i]->toString();
    }
}

void test_intf() {
    std::multimap<std::string, std::pair<chat::Address::ptr, uint32_t> > res;
    bool v = chat::Address::GetInterfaceAddresses(res);
    if (!v) {
        CHAT_LOG_ERROR(g_logger) << "GetInterfaceAddresses fail";
        return;
    }
    for (auto& i : res) {
        CHAT_LOG_INFO(g_logger) << i.first << "-" << i.second.first->toString() << "-" << i.second.second;
    }
}

void test_ipv4() {
    //auto addr = chat::IPAddress::Create("www.baidu.com");
    auto addr = chat::IPAddress::Create("36.155.132.31");
    if (addr) {
        CHAT_LOG_INFO(g_logger) << addr->toString();
    }
}

int main(int argc, char** argv) {
    //test();
    //test_intf();
    test_ipv4();
    return 0;
}