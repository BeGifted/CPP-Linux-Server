#include "../chat/daemon.h"
#include "../chat/iomanager.h"
#include "../chat/log.h"

static chat::Logger::ptr g_logger = CHAT_LOG_ROOT();

chat::Timer::ptr timer;
int server_main(int argc, char** argv) {
    CHAT_LOG_INFO(g_logger) << chat::ProcessInfoMgr::GetInstance()->toString();
    chat::IOManager iom(1);
    timer = iom.addTimer(1000, [](){
            CHAT_LOG_INFO(g_logger) << "onTimer";
            static int count = 0;
            if(++count > 10) {
                exit(1);
            }
    }, true);
    return 0;
}

int main(int argc, char** argv) {
    std::cout << argc << std::endl;
    return chat::start_daemon(argc, argv, server_main, argc != 1);
}