#ifndef __CHAT_FIBER_H__
#define __CHAT_FIBER_H__

#include <ucontext.h>
#include <memory>
#include <functional>
#include "thread.h"

namespace chat {

class Fiber: public std::enable_shared_from_this<Fiber> {
public:
    typedef std::shared_ptr<Fiber> ptr;

    enum State {
        INIT,
        HOLD,
        EXEC,
        TERM,
        READY,
        EXCEPT
    };
private:
    Fiber();

public:
    Fiber(std::function<void()> cb, size_t stacksize = 0);
    ~Fiber();

    void reset(std::function<void()> cb); //init term except 状态重置
    void swapIn();  //切换到当前协程
    void swapOut();  //切换到后台
    uint64_t getId() const { return m_id; }

public:
    //设置当前协程
    static void SetThis(Fiber* f);
    //返回当前协程
    static Fiber::ptr GetThis();
    //协程切换到后台
    static void YieldToReady();
    static void YieldToHold();
    //总协程数
    static uint64_t TotalFibers();

    static void MainFunc();

    static uint64_t GetFiberId();
private:
    uint64_t m_id = 0;
    uint32_t m_stacksize = 0;
    State m_state = State::INIT;

    ucontext_t m_ctx;
    void* m_stack = nullptr;

    std::function<void()> m_cb;
};

}


#endif