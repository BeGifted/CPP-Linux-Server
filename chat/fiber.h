#ifndef __CHAT_FIBER_H__
#define __CHAT_FIBER_H__

#include <memory>
#include <functional>

#define FIBER_UCONTEXT      1
#define FIBER_FCONTEXT      2
#define FIBER_LIBCO         3 
#define FIBER_LIBACO        4

#ifndef FIBER_CONTEXT_TYPE
#define FIBER_CONTEXT_TYPE FIBER_LIBCO
#endif

#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT
#include <ucontext.h>
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
#include "fcontext/fcontext.h"
#elif FIBER_CONTEXT_TYPE == FIBER_LIBCO
#include "libco/coctx.h"
#elif FIBER_CONTEXT_TYPE == FIBER_LIBACO
#include "libaco/aco.h"
#endif

namespace chat {

class Scheduler;
class Fiber;
Fiber* NewFiber();
Fiber* NewFiber(std::function<void()> cb, size_t stacksize = 0, bool use_caller = false);
void FreeFiber(Fiber* ptr);

class Fiber: public std::enable_shared_from_this<Fiber> {
friend class Scheduler;
friend Fiber* NewFiber();
friend Fiber* NewFiber(std::function<void()> cb, size_t stacksize, bool use_caller);
friend void FreeFiber(Fiber* ptr);

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
    Fiber(std::function<void()> cb, size_t stacksize = 0, bool use_caller = false);

public:
    ~Fiber();

    void reset(std::function<void()> cb); //init term except 状态重置
    void swapIn();  //切换到当前协程
    void swapOut();  //切换到后台
    uint64_t getId() const { return m_id; }
    State getState() const { return m_state;}
    void call();
    void back();
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

#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT || FIBER_CONTEXT_TYPE == FIBER_LIBACO
    //执行完成返回到线程主协程
    static void MainFunc();
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
    static void MainFunc(intptr_t vp);
#elif FIBER_CONTEXT_TYPE == FIBER_LIBCO
    static void* MainFunc(void*, void*);
#endif

#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT || FIBER_CONTEXT_TYPE == FIBER_LIBACO
    //执行完成返回到线程调度协程
    static void CallerMainFunc();
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
    static void CallerMainFunc(intptr_t vp);
#elif FIBER_CONTEXT_TYPE == FIBER_LIBCO
    static void* CallerMainFunc(void*, void*);
#endif

    static uint64_t GetFiberId();
private:
    uint64_t m_id = 0;
    uint32_t m_stacksize = 0;
    State m_state = State::INIT;

#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT
    ucontext_t m_ctx;
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
    fcontext_t m_ctx = nullptr;
#elif FIBER_CONTEXT_TYPE == FIBER_LIBCO
    coctx_t m_ctx;
#elif FIBER_CONTEXT_TYPE == FIBER_LIBACO
    aco_t* m_ctx = nullptr;
    aco_share_stack_t m_astack;
#endif

    void* m_stack = nullptr;  // 栈指针
    std::function<void()> m_cb;
};

}


#endif