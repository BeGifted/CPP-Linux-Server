#include "../chat/chat.h"

chat::Logger::ptr g_logger = CHAT_LOG_ROOT();

int count = 0;
// chat::RWMutex s_mutex;
chat::Mutex s_mutex;

void fun1() {
    CHAT_LOG_INFO(g_logger) << "name: " << chat::Thread::GetName()
                            << " this.name: " << chat::Thread::GetThis()->getName()
                            << " id: " << chat::GetThreadId()
                            << " this.id: " << chat::Thread::GetThis()->getId();
    for (int i = 0; i < 100000; i++) {
        //chat::RWMutex::WriteLock lock(s_mutex);
        chat::Mutex::Lock lock(s_mutex);
        count++;
    }
}

void fun2() {
    while(1){
        CHAT_LOG_INFO(g_logger) << "xxxxxxxxxxx";
    }
}

void fun3() {
    while(1){
        CHAT_LOG_INFO(g_logger) << "===========";
    }
}

int main(int argc, char** argv) {
    CHAT_LOG_INFO(g_logger) << "thread test begin";
    YAML::Node root = YAML::LoadFile("/root/serverProjects/cpp_chatroom_server/bin/conf/log2.yml");
    chat::Config::LoadFromYaml(root);

    std::vector<chat::Thread::ptr> thrs;
    for (int i = 0; i < 5; i++) {
        chat::Thread::ptr thr(new chat::Thread(&fun1, "name_" + std::to_string(i * 2)));
        chat::Thread::ptr thr2(new chat::Thread(&fun2, "name_" + std::to_string(i * 2 + 1)));
        
        thrs.push_back(thr);
        thrs.push_back(thr2);
    }
    for (size_t i = 0; thrs.size(); i++) {
        thrs[i]->join();
    }
    CHAT_LOG_INFO(g_logger) << "thread test end";
    CHAT_LOG_INFO(g_logger) << "count=" << count;

    return 0;
}