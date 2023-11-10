#include "../chat/chat.h"

chat::Logger::ptr g_logger = CHAT_LOG_ROOT();

void test_fiber() {
    static int count = 5;
    CHAT_LOG_INFO(g_logger) << "test in fiber, count=" << count;

    sleep(1);
    if(--count >= 0) {
        chat::Scheduler::GetThis()->schedule(&test_fiber, chat::GetThreadId());
    }
}

int main(int argc, char** argv) {
    CHAT_LOG_INFO(g_logger) << "main";
    chat::Scheduler sc(3, false, "test");
    sc.start();
    sleep(2);
    CHAT_LOG_INFO(g_logger) << "scheduler";
    sc.schedule(&test_fiber);
    sc.stop();
    CHAT_LOG_INFO(g_logger) << "over";
    return 0;
}