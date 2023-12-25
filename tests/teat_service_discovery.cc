#include "chat/chat.h"

void run() {
    chat::ZKServiceDiscovery::ptr zksd(new chat::ZKServiceDiscovery("127.0.0.1:2388"));
    zksd->registerServer("blog", "chat", "127.0.0.1:1111", "abcd");
    // zksd->registerServer("blog", "chat2", "127.0.0.1:1111", "abcd");
    // zksd->setServiceCallback()  // TODO
    zksd->queryServer("blog", "chat");
    zksd->setSelfInfo("127.0.0.1:2222");
    zksd->setSelfData("aaaa");

    zksd->start();
}

int main(int argc, char** argv) {
    chat::IOManager iom(1);
    iom.addTimer(1000, [](){}, true);
    iom.schedule(run);
    return 0;
}