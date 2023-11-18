#include "../chat/uri.h"
#include <iostream>

int main(int argc, char** argv) {
    //chat::Uri::ptr uri = chat::Uri::Create("http://www.baidu.com/test/uri?id=100&name=chat#frg");
    chat::Uri::ptr uri = chat::Uri::Create("http://admin@www.baidu.com/test/中文/uri?id=100&name=chat&vv=中文#frg中文");
    //chat::Uri::ptr uri = chat::Uri::Create("http://admin@www.baidu.com");
    //chat::Uri::ptr uri = chat::Uri::Create("http://www.baidu.com/test/uri");
    std::cout << uri->toString() << std::endl;
    auto addr = uri->createAddress();
    std::cout << addr->toString() << std::endl;
    return 0;
}