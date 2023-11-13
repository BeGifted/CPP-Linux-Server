#ifndef __CHAT_NONCOPYABLE_H__
#define __CHAT_NONCOPYABLE_H__

namespace chat {


//对象无法拷贝,赋值
class Noncopyable {
public:

    Noncopyable() = default;
    ~Noncopyable() = default;

    Noncopyable(const Noncopyable&) = delete;
    Noncopyable& operator=(const Noncopyable&) = delete;
};

}

#endif