#include "../chat/chat.h"

chat::Logger::ptr g_logger = CHAT_LOG_ROOT();

void test_fiber() {
    CHAT_LOG_INFO(g_logger) << "test in fiber";
}

int main(int argc, char** argv) {
    CHAT_LOG_INFO(g_logger) << "main";
    chat::Scheduler sc;
    sc.start();
    sc.schedule(&test_fiber);
    sc.stop();
    CHAT_LOG_INFO(g_logger) << "over";
    return 0;
}