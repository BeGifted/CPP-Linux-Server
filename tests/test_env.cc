#include "chat/env.h"
#include <unistd.h>
#include <iostream>
#include <fstream>

struct A {
    A() {
        std::ifstream ifs("/proc/" + std::to_string(getpid()) + "/cmdline", std::ios::binary);
        std::string content;
        content.resize(4096);

        ifs.read(&content[0], content.size());
        content.resize(ifs.gcount());

        for(size_t i = 0; i < content.size(); ++i) {
            std::cout << i << " - " << content[i] << " - " << (int)content[i] << std::endl;
        }
    }
};

A a;

int main(int argc, char** argv) {
    std::cout << "argc=" << argc << std::endl;
    chat::EnvMgr::GetInstance()->addHelp("s", "start with the terminal");
    chat::EnvMgr::GetInstance()->addHelp("d", "run as daemon");
    chat::EnvMgr::GetInstance()->addHelp("p", "print help");
    if (!chat::EnvMgr::GetInstance()->init(argc, argv)) {
        chat::EnvMgr::GetInstance()->printHelp();
        return 0;
    }

    std::cout << "exe=" << chat::EnvMgr::GetInstance()->getExe() << std::endl;
    std::cout << "cwd=" << chat::EnvMgr::GetInstance()->getCwd() << std::endl;

    std::cout << "path=" << chat::EnvMgr::GetInstance()->getEnv("PATH", "xxx") << std::endl;
    std::cout << "test=" << chat::EnvMgr::GetInstance()->getEnv("TEST", "") << std::endl;
    std::cout << "set env " << chat::EnvMgr::GetInstance()->setEnv("TEST", "yy") << std::endl;
    std::cout << "test=" << chat::EnvMgr::GetInstance()->getEnv("TEST", "") << std::endl;
    if (chat::EnvMgr::GetInstance()->has("p")) {
        chat::EnvMgr::GetInstance()->printHelp();
    }
    return 0;
}