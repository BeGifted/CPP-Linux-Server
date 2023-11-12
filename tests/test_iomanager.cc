#include "../chat/chat.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

chat::Logger::ptr g_logger = CHAT_LOG_ROOT();

int sock = 0;

void test_fiber() {
    CHAT_LOG_INFO(g_logger) << "test_fiber sock=" << sock;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(sock, F_SETFL, O_NONBLOCK);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "115.239.210.27", &addr.sin_addr.s_addr);

    if(!connect(sock, (const sockaddr*)&addr, sizeof(addr))) {
        CHAT_LOG_ERROR(g_logger) << "connection fail";
    } else if(errno == EINPROGRESS) {
        CHAT_LOG_INFO(g_logger) << "add event errno=" << errno << " " << strerror(errno);
        chat::IOManager::GetThis()->addEvent(sock, chat::IOManager::READ, [](){
            CHAT_LOG_INFO(g_logger) << "read callback";
        });
        
        chat::IOManager::GetThis()->addEvent(sock, chat::IOManager::WRITE, [](){
            CHAT_LOG_INFO(g_logger) << "write callback";
            //close(sock);
            chat::IOManager::GetThis()->cancelEvent(sock, chat::IOManager::READ);
            close(sock);
        });
    } else {
        CHAT_LOG_INFO(g_logger) << "else " << errno << " " << strerror(errno);
    }

}

void test1() {
    chat::IOManager iom(2, false);
    iom.schedule(&test_fiber);
}

chat::Timer::ptr s_timer;
void test_timer() {
    chat::IOManager iom(2);
    s_timer = iom.addTimer(1000, [](){
        static int i = 0;
        CHAT_LOG_INFO(g_logger) << "hello timer i=" << i;
        if(++i == 3) {
            //s_timer->reset(2000, true);
            s_timer->cancel();
        }
    }, true);
}


int main(int argc, char** argv) {
    //test1();
    test_timer();
    return 0;
}