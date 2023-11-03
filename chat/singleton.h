#ifndef __CHAT_SINGLETON_H__
#define __CHAT_SINGLETON_H__

#include <memory>
namespace chat {

template<class T, class X = void, int N = 0>
class Singleton {
public:
    static T* GetInstance() {
        static T v;
        return &v;  //单例裸指针
    }
};

template<class T, class X = void, int N = 0>
class SingletonPtr {
public:
    static std::shared_ptr<T> GetInstance() {
        static std::shared_ptr<T> v(new T);
        return v;  //单例智能指针
    }
};
}
#endif