#include "fiber.h"
#include <atomic>
#include "config.h"
#include "macro.h"

namespace chat {
static std::atomic<uint64_t> s_fiber_id {0};
static std::atomic<uint64_t> s_fiber_count {0};

static thread_local Fiber* t_fiber = nullptr;
static thread_local Fiber::ptr t_threadFiber = nullptr;

static ConfigVar<uint32_t>::ptr g_fiber_stack_size = 
    Config::Lookup<uint32_t>("fiber.stack_size", 1024 * 1024, "fiber stack size");

static Logger::ptr g_logger = CHAT_LOG_NAME("system");


class MallocStackAllocator {
public:
    static void* Alloc(size_t size) {
        return malloc(size);
    }

    static void Dealloc(void* vp, size_t size) {
        return free(vp);
    }
};

using StackAllocator = MallocStackAllocator;

Fiber::Fiber() {  //主协程
    m_state = State::EXEC;
    SetThis(this);

    if (getcontext(&m_ctx)) {
        CHAT_ASSERT2(false, "getcontext");
    }

    ++s_fiber_count;

    CHAT_LOG_DEBUG(g_logger) << "Fiber::Fiber";
}

//新的协程
Fiber::Fiber(std::function<void()> cb, size_t stacksize) 
    :m_id(++s_fiber_id)
    ,m_cb(cb){
    ++s_fiber_count;
    m_stacksize = stacksize ? stacksize : g_fiber_stack_size->getValue();

    m_stack = StackAllocator::Alloc(m_stacksize);
    if (getcontext(&m_ctx)) {
        CHAT_ASSERT2(false, "getcontext");
    }
    m_ctx.uc_link = nullptr;  //执行完毕后，不会自动跳转到另一个上下文
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stacksize;

    makecontext(&m_ctx, &Fiber::MainFunc, 0);

    CHAT_LOG_DEBUG(g_logger) << "Fiber::Fiber id=" << m_id;
}

Fiber::~Fiber() {
    --s_fiber_count;
    if (m_stack) {
        CHAT_ASSERT(m_state == State::TERM || m_state == State::INIT || m_state == State::EXCEPT);
        StackAllocator::Dealloc(m_stack, m_stacksize);
    } else {
        CHAT_ASSERT(!m_cb);
        CHAT_ASSERT(m_state == State::EXEC);

        Fiber* cur = t_fiber;
        if (cur == this) {
            SetThis(nullptr);
        }
    }
    CHAT_LOG_DEBUG(g_logger) << "Fiber::~Fiber id=" << m_id;
}

uint64_t Fiber::GetFiberId() {
    if (t_fiber) {
        return t_fiber->getId();
    }
    return 0;
}

void Fiber::reset(std::function<void()> cb) {
    CHAT_ASSERT(m_stack);
    CHAT_ASSERT(m_state == State::TERM || m_state == State::INIT || m_state == State::EXCEPT);
    m_cb = cb;
    if (getcontext(&m_ctx)) {
        CHAT_ASSERT2(false, "getcontext");
    }

    m_ctx.uc_link = nullptr;  //执行完毕后，不会自动跳转到另一个上下文
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stacksize;

    makecontext(&m_ctx, &Fiber::MainFunc, 0);
    m_state = State::INIT;
}

void Fiber::swapIn() {
    SetThis(this);
    CHAT_ASSERT(m_state != State::EXEC);
    m_state = State::EXEC;

    if (swapcontext(&t_threadFiber->m_ctx, &m_ctx)) {
        CHAT_ASSERT2(false, "getcontext");
    }
}

void Fiber::swapOut() {
    SetThis(t_threadFiber.get());

    if (swapcontext(&m_ctx, &t_threadFiber->m_ctx)){
        CHAT_ASSERT2(false, "getcontext");
    }
}

//返回当前协程
Fiber::ptr Fiber::GetThis() {
    if (t_fiber) {
        return t_fiber->shared_from_this();  //指向自身的智能指针
    }
    Fiber::ptr main_fiber(new Fiber);
    CHAT_ASSERT(t_fiber == main_fiber.get());
    t_threadFiber = main_fiber;
    return t_fiber->shared_from_this();
}

void Fiber::SetThis(Fiber* f) {
    t_fiber = f;
}

//协程切换到后台
void Fiber::YieldToReady() {
    Fiber::ptr cur = GetThis();
    CHAT_ASSERT(cur->m_state == State::EXEC);
    cur->m_state = State::READY;
    cur->swapOut();
}
void Fiber::YieldToHold() {
    Fiber::ptr cur = GetThis();
    CHAT_ASSERT(cur->m_state == State::EXEC);
    cur->m_state = State::HOLD;
    cur->swapOut();
}

//总协程数
uint64_t Fiber::TotalFibers() {
    return s_fiber_count;
}

void Fiber::MainFunc() {
    Fiber::ptr cur = GetThis();
    CHAT_ASSERT(cur);
    try {
        cur->m_cb();
        cur->m_cb = nullptr;
        cur->m_state = State::TERM;
    } catch(std::exception& ex) {
        cur->m_state = State::EXCEPT;
        CHAT_LOG_ERROR(g_logger) << "Fiber Except: " << ex.what();
    } catch(...) {
        cur->m_state = State::EXCEPT;
        CHAT_LOG_ERROR(g_logger) << "Fiber Except: ";
    }

    auto raw_ptr = cur.get();
    cur.reset();
    raw_ptr->swapOut();

    CHAT_ASSERT2(false, "never reach");
}


}