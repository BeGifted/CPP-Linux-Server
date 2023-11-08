#include "../chat/chat.h"

chat::Logger::ptr g_logger = CHAT_LOG_ROOT();

void run_in_fiber() {
    CHAT_LOG_INFO(g_logger) << "run_in_fiber begin";
    chat::Fiber::YieldToHold();
    CHAT_LOG_INFO(g_logger) << "run_in_fiber end";
    chat::Fiber::YieldToHold();
}
void test_fiber() {
    CHAT_LOG_INFO(g_logger) << "main begin -1";
    {
        chat::Fiber::GetThis();
        CHAT_LOG_INFO(g_logger) << "main begin";
        chat::Fiber::ptr fiber(new chat::Fiber(run_in_fiber));
        fiber->swapIn();  //call mainfunc->cb
        CHAT_LOG_INFO(g_logger) << "main after swapIn";
        fiber->swapIn();
        CHAT_LOG_INFO(g_logger) << "main after end";
        fiber->swapIn();
    }
    CHAT_LOG_INFO(g_logger) << "main after end2";
}

int main(int argc, char** argv) {
    chat::Thread::SetName("main");

    std::vector<chat::Thread::ptr> thrs;
    for (int i = 0; i < 3; ++i) {
        thrs.push_back(chat::Thread::ptr(
            new chat::Thread(&test_fiber, "name_" + std::to_string(i))));
    }
    for (auto i : thrs) {
        i->join();
    }
    return 0;
}