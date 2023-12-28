#include "fiber.h"
#include "config.h"
#include "macro.h"
#include "scheduler.h"
#include <atomic>
#include <sys/mman.h>

namespace chat {
static std::atomic<uint64_t> s_fiber_id {0};
static std::atomic<uint64_t> s_fiber_count {0};

static thread_local Fiber* t_fiber = nullptr;
static thread_local Fiber::ptr t_threadFiber = nullptr;

static ConfigVar<uint32_t>::ptr g_fiber_stack_size = 
    Config::Lookup<uint32_t>("fiber.stack_size", 32 * 1024, "fiber stack size");

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

class MMapStackAllocator {
public:
    static void* Alloc(size_t size) {
        return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    }

    static void Dealloc(void* vp, size_t size) {
        munmap(vp, size);
    }
};

using StackAllocator = MallocStackAllocator;

Fiber* NewFiber() {
    return new Fiber();
}

Fiber* NewFiber(std::function<void()> cb, size_t stacksize, bool use_caller) {
    stacksize = stacksize ? stacksize : g_fiber_stack_size->getValue();
    Fiber* p = (Fiber*)StackAllocator::Alloc(sizeof(Fiber) + stacksize);
    return new (p) Fiber(cb, stacksize, use_caller);
}

void FreeFiber(Fiber* ptr) {
    ptr->~Fiber();
    StackAllocator::Dealloc(ptr, 0);
}


Fiber::Fiber() {  //主协程
    m_state = State::EXEC;
    SetThis(this);

#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT
    if (getcontext(&m_ctx)) {
        CHAT_ASSERT2(false, "getcontext");
    }
#elif FIBER_CONTEXT_TYPE == FIBER_LIBCO
    coctx_init(&m_ctx);
#elif FIBER_CONTEXT_TYPE == FIBER_LIBACO
    aco_thread_init(nullptr);
    m_ctx = aco_create(nullptr, nullptr, 0, nullptr, nullptr);
#endif

    ++s_fiber_count;

    CHAT_LOG_DEBUG(g_logger) << "Fiber::Fiber";
}

//新的协程
Fiber::Fiber(std::function<void()> cb, size_t stacksize, bool use_caller) 
    :m_id(++s_fiber_id)
    ,m_cb(cb){
    ++s_fiber_count;
    m_stacksize = stacksize;

#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT
    if (getcontext(&m_ctx)) {
        CHAT_ASSERT2(false, "getcontext");
    }

    m_ctx.uc_link = nullptr;  //执行完毕后，不会自动跳转到另一个上下文
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stacksize;

    if(!use_caller) {
        makecontext(&m_ctx, &Fiber::MainFunc, 0);
    } else {
        makecontext(&m_ctx, &Fiber::CallerMainFunc, 0);
    }
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
    if(!use_caller) {
        m_ctx = make_fcontext((char*)m_stack + m_stacksize, m_stacksize, &Fiber::MainFunc);
    } else {
        m_ctx = make_fcontext((char*)m_stack + m_stacksize, m_stacksize, &Fiber::CallerMainFunc);
    }
#elif FIBER_CONTEXT_TYPE == FIBER_LIBCO
    m_ctx.ss_size = m_stacksize;
    m_ctx.ss_sp = (char*)m_stack;
    if(!use_caller) {
        coctx_make(&m_ctx, &Fiber::MainFunc, 0, 0);
    } else {
        coctx_make(&m_ctx, &Fiber::CallerMainFunc, 0, 0);
    }
#elif FIBER_CONTEXT_TYPE == FIBER_LIBACO
    aco_share_stack_init(&m_astack, m_stack, m_stacksize);
    m_ctx = aco_create(t_threadFiber->m_ctx, &m_astack, 0, &Fiber::MainFunc, nullptr);
#endif

    CHAT_LOG_DEBUG(g_logger) << "Fiber::Fiber id=" << m_id;
}

Fiber::~Fiber() {
    --s_fiber_count;
    if (m_stack) {
        CHAT_ASSERT(m_state == State::TERM || m_state == State::INIT || m_state == State::EXCEPT);
    } else {
        CHAT_ASSERT(!m_cb);
        CHAT_ASSERT(m_state == State::EXEC);

        Fiber* cur = t_fiber;
        if (cur == this) {
            SetThis(nullptr);
        }
    }
    CHAT_LOG_DEBUG(g_logger) << "Fiber::~Fiber id=" << m_id << " total=" << s_fiber_count;
}

uint64_t Fiber::GetFiberId() {
    if (t_fiber) {
        return t_fiber->getId();
    }
    return 0;
}

//重置协程函数 + 状态
void Fiber::reset(std::function<void()> cb) {
    CHAT_ASSERT(m_stack);
    CHAT_ASSERT(m_state == State::TERM || m_state == State::INIT || m_state == State::EXCEPT);
    m_cb = cb;

#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT
    if (getcontext(&m_ctx)) {
        CHAT_ASSERT2(false, "getcontext");
    }

    m_ctx.uc_link = nullptr;  //执行完毕后，不会自动跳转到另一个上下文
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stacksize;

    makecontext(&m_ctx, &Fiber::MainFunc, 0);
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
    m_ctx = make_fcontext((char*)m_stack + m_stacksize, m_stacksize, &Fiber::MainFunc);
#elif FIBER_CONTEXT_TYPE == FIBER_LIBCO
    coctx_make(&m_ctx, &Fiber::MainFunc, 0, 0);
#elif FIBER_CONTEXT_TYPE == FIBER_LIBACO
    m_ctx = aco_create(t_threadFiber->m_ctx, &m_astack, 0, &Fiber::MainFunc, nullptr);
#endif

    m_state = State::INIT;
}

void Fiber::call() {
    SetThis(this);
    m_state = State::EXEC;

#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT
    if (swapcontext(&t_threadFiber->m_ctx, &m_ctx)) {
        CHAT_ASSERT2(false, "getcontext");
    }
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
    jump_fcontext(&t_threadFiber->m_ctx, m_ctx, 0);
#elif FIBER_CONTEXT_TYPE == FIBER_LIBCO
    coctx_swap(&t_threadFiber->m_ctx, &m_ctx);
#elif FIBER_CONTEXT_TYPE == FIBER_LIBACO
    acosw(t_threadFiber->m_ctx, m_ctx);
#endif
}
void Fiber::swapIn() {
    SetThis(this);
    m_state = State::EXEC;

#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT
    if (swapcontext(&Scheduler::GetMainFiber()->m_ctx, &m_ctx)) {
        CHAT_ASSERT2(false, "getcontext");
    }
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
    jump_fcontext(&Scheduler::GetMainFiber()->m_ctx, m_ctx, 0);
#elif FIBER_CONTEXT_TYPE == FIBER_LIBCO
    coctx_swap(&Scheduler::GetMainFiber()->m_ctx, &m_ctx);
#elif FIBER_CONTEXT_TYPE == FIBER_LIBACO
    acosw(Scheduler::GetMainFiber()->m_ctx, m_ctx);
#endif
}

void Fiber::back() {
    SetThis(t_threadFiber.get());

#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT
    if(swapcontext(&m_ctx, &t_threadFiber->m_ctx)) {
        CHAT_ASSERT2(false, "swapcontext");
    }
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
    jump_fcontext(&m_ctx, t_threadFiber->m_ctx, 0);
#elif FIBER_CONTEXT_TYPE == FIBER_LIBCO
    coctx_swap(&m_ctx, &t_threadFiber->m_ctx);
#elif FIBER_CONTEXT_TYPE == FIBER_LIBACO
    acosw(m_ctx, t_threadFiber->m_ctx);
#endif
}

void Fiber::swapOut() {
    SetThis(Scheduler::GetMainFiber());

#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT
    if (swapcontext(&m_ctx, &Scheduler::GetMainFiber()->m_ctx)){
        CHAT_ASSERT2(false, "getcontext");
    }
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
    jump_fcontext(&m_ctx, Scheduler::GetMainFiber()->m_ctx, 0);
#elif FIBER_CONTEXT_TYPE == FIBER_LIBCO
    coctx_swap(&m_ctx, &Scheduler::GetMainFiber()->m_ctx);
#elif FIBER_CONTEXT_TYPE == FIBER_LIBACO
    acosw(m_ctx, Scheduler::GetMainFiber()->m_ctx);
#endif
}

//返回当前协程
Fiber::ptr Fiber::GetThis() {
    if (t_fiber) {
        return t_fiber->shared_from_this();  //指向自身的智能指针
    }
    Fiber::ptr main_fiber(NewFiber());
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

#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT || FIBER_CONTEXT_TYPE == FIBER_LIBACO
void Fiber::MainFunc() {
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
void Fiber::MainFunc(intptr_t vp) {
#elif FIBER_CONTEXT_TYPE == FIBER_LIBCO
void* Fiber::MainFunc(void*, void*) {
#endif
    Fiber::ptr cur = GetThis();
    CHAT_ASSERT(cur);
    try {
        cur->m_cb();
        cur->m_cb = nullptr;
        cur->m_state = State::TERM;
    } catch(std::exception& ex) {
        cur->m_state = State::EXCEPT;
        CHAT_LOG_ERROR(g_logger) << "Fiber Except: " << ex.what()
            << " fiber id=" << cur->getId()
            << std::endl
            << chat::BacktraceToString();
    } catch(...) {
        cur->m_state = State::EXCEPT;
        CHAT_LOG_ERROR(g_logger) << "Fiber Except: ";
    }

    auto raw_ptr = cur.get();
    cur.reset();
    raw_ptr->swapOut();

    CHAT_ASSERT2(false, "never reach, fiber id=" + std::to_string(raw_ptr->getId()));
}

#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT || FIBER_CONTEXT_TYPE == FIBER_LIBACO
void Fiber::CallerMainFunc() {
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
void Fiber::CallerMainFunc(intptr_t vp) {
#elif FIBER_CONTEXT_TYPE == FIBER_LIBCO
void* Fiber::CallerMainFunc(void*, void*) {
#endif
    Fiber::ptr cur = GetThis();
    CHAT_ASSERT(cur);
    try {
        cur->m_cb();
        cur->m_cb = nullptr;
        cur->m_state = State::TERM;
    } catch (std::exception& ex) {
        cur->m_state = State::EXCEPT;
        CHAT_LOG_ERROR(g_logger) << "Fiber Except: " << ex.what()
            << " fiber_id=" << cur->getId()
            << std::endl
            << chat::BacktraceToString();
    } catch (...) {
        cur->m_state = State::EXCEPT;
        CHAT_LOG_ERROR(g_logger) << "Fiber Except"
            << " fiber_id=" << cur->getId()
            << std::endl
            << chat::BacktraceToString();
    }

    auto raw_ptr = cur.get();
    cur.reset();
    raw_ptr->back();

    CHAT_ASSERT2(false, "never reach fiber_id=" + std::to_string(raw_ptr->getId()));
}


}