#include "scheduler.h"
#include "log.h"
#include "macro.h"
#include "hook.h"

namespace chat {

static chat::Logger::ptr g_logger = CHAT_LOG_NAME("system");

static thread_local Scheduler* t_scheduler = nullptr;
static thread_local Fiber* t_scheduler_fiber = nullptr;

Scheduler::Scheduler(size_t threads, bool use_caller, const std::string& name)
    :m_name(name) {
    CHAT_ASSERT(threads > 0);

    if (use_caller) {
        chat::Fiber::GetThis();  //没有协程会创建主协程
        --threads;

        CHAT_ASSERT(GetThis() == nullptr);
        t_scheduler = this;

        m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true));
        chat::Thread::SetName(m_name);

        t_scheduler_fiber = m_rootFiber.get();
        m_rootThread = chat::GetThreadId();
        m_threadIds.push_back(m_rootThread);
    } else{
        m_rootThread = -1;
    }
    m_threadCount = threads;

}

Scheduler::~Scheduler() {
    CHAT_ASSERT(m_stopping);
    if (GetThis() == this) {
        t_scheduler = nullptr;
    }
}

Scheduler* Scheduler::GetThis() {
    return t_scheduler;
}

Fiber* Scheduler::GetMainFiber() {
    return t_scheduler_fiber;
}

void Scheduler::start() {
    MutexType::WriteLock lock(m_mutex);
    if (!m_stopping) {
        return;
    }
    m_stopping = false;

    CHAT_ASSERT(m_threads.empty());

    m_threads.resize(m_threadCount);
    for (size_t i = 0; i < m_threadCount; i++) {
        m_threads[i] = std::make_shared<Thread>(std::bind(&Scheduler::run, this), m_name + "_" + std::to_string(i));
        m_threadIds.push_back(m_threads[i]->getId());
    }
    lock.unlock();

    // if (m_rootFiber) {
    //     //m_rootFiber->swapIn();
    //     m_rootFiber->call();
    //     CHAT_LOG_INFO(g_logger) << "call out, state=" << m_rootFiber->getState();
    // }
}

void Scheduler::stop() {
    m_autoStop = true;
    if (m_rootFiber 
        && m_threadCount == 0 
        && (m_rootFiber->getState() == Fiber::State::TERM || m_rootFiber->getState() == Fiber::State::INIT)) {
        CHAT_LOG_INFO(g_logger) << this << " stopped";
        m_stopping = true;

        if (stopping()) {
            return;
        }
    }

    // bool exit_on_this_fiber = false;
    if (m_rootThread != -1) {
        CHAT_ASSERT(GetThis() == this);
    } else {
        CHAT_ASSERT(GetThis() != this);
    }

    m_stopping = true;
    for (size_t i = 0; i < m_threadCount; i++) {
        tickle();
    }

    if (m_rootFiber) {
        tickle();
    }

    if (m_rootFiber) {
        // while(!stopping()) {
        //     if(m_rootFiber->getState() == Fiber::State::TERM
        //         || m_rootFiber->getState() == Fiber::State::EXCEPT) {
        //         m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true));
        //         CHAT_LOG_INFO(g_logger) << "root fiber terminates, reset";
        //         t_scheduler_fiber = m_rootFiber.get();
        //     }
        //     m_rootFiber->call();
        // }
        if (!stopping()) {
            m_rootFiber->call();
        }
    }

    std::vector<Thread::ptr> thrs;
    {
        MutexType::WriteLock lock(m_mutex);
        thrs.swap(m_threads);
    }

    for (auto& i : thrs) {
        i->join();
    }

    // if (exit_on_this_fiber) {

    // }
}

void Scheduler::setThis(){
    t_scheduler = this;
}

void Scheduler::run() {  // 新创建线程执行
    CHAT_LOG_DEBUG(g_logger) << m_name << " run";
    set_hook_enable(true);
    setThis();
    if (chat::GetThreadId() != m_rootThread) {
        t_scheduler_fiber = Fiber::GetThis().get();
    }

    Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this)));
    Fiber::ptr cb_fiber;

    FiberAndThread ft;
    while (true) {
        ft.reset();
        bool tickle_me = false;
        bool is_active = false;
        {   //消息队列取出需要执行的任务
            MutexType::WriteLock lock(m_mutex);
            auto it = m_fibers.begin();
            while (it != m_fibers.end()) { 
                if(it->thread != -1 && it->thread != chat::GetThreadId()) {
                    ++it;
                    tickle_me = true;
                    continue;
                }

                CHAT_ASSERT(it->fiber || it->cb);
                if(it->fiber && it->fiber->getState() == Fiber::State::EXEC) {
                    ++it;
                    continue;
                }

                ft = *it;
                m_fibers.erase(it++);
                ++m_activeThreadCount;
                is_active = true;
                break;
            }
            tickle_me |= !m_fibers.empty();
        }

        if (tickle_me) {
            tickle();
        }

        if (ft.fiber 
            && (ft.fiber->getState() != Fiber::State::TERM 
                && ft.fiber->getState() != Fiber::State::EXCEPT)) {
            // ++m_activeThreadCount;
            ft.fiber->swapIn();  //工作线程
            --m_activeThreadCount;

            if (ft.fiber->getState() == Fiber::READY) {
                schedule(ft.fiber);
            } else if (ft.fiber->getState() != Fiber::State::TERM
                        && ft.fiber->getState() != Fiber::State::EXCEPT) {
                ft.fiber->m_state = Fiber::State::HOLD;
            }
        } else if (ft.cb) {
            if (cb_fiber) {
                cb_fiber->reset(ft.cb);
            } else {
                cb_fiber.reset(new Fiber(ft.cb));
            }
            ft.reset();

            // ++m_activeThreadCount;
            cb_fiber->swapIn();
            --m_activeThreadCount;

            if (cb_fiber->getState() == Fiber::State::READY) {
                schedule(cb_fiber);
                cb_fiber.reset();
            } else if (cb_fiber->getState() == Fiber::State::EXCEPT || cb_fiber->getState() == Fiber::State::TERM) {
                cb_fiber->reset(nullptr);
            } else {
                cb_fiber->m_state = Fiber::State::HOLD;
                cb_fiber.reset();
            }
        } else {  //无任务执行idle
            if (is_active) {
                --m_activeThreadCount;
                continue;
            }
            if (idle_fiber->getState() == Fiber::State::TERM) {
                CHAT_LOG_INFO(g_logger) << "idle fiber terminates";
                break;
            }

            ++m_idleThreadCount;
            idle_fiber->swapIn();  //空闲协程
            --m_idleThreadCount;
            if (idle_fiber->getState() != Fiber::State::TERM 
                && idle_fiber->getState() != Fiber::State::EXCEPT) {
                idle_fiber->m_state = Fiber::State::HOLD;
            }
        }
    }
}

void Scheduler::tickle() {
    CHAT_LOG_INFO(g_logger) << "tickle";
}

bool Scheduler::stopping() {
    MutexType::ReadLock lock(m_mutex);
    return m_stopping && m_autoStop && m_fibers.empty() && m_activeThreadCount == 0;
}

void Scheduler::idle() {
    CHAT_LOG_INFO(g_logger) << "idle";
    while (!stopping()) {
        chat::Fiber::YieldToHold();
    }
}

void Scheduler::switchTo(int thread) {
    CHAT_ASSERT(Scheduler::GetThis() != nullptr);
    if (Scheduler::GetThis() == this) {
        if (thread == -1 || thread == chat::GetThreadId()) {
            return;
        }
    }
    schedule(Fiber::GetThis(), thread);
    Fiber::YieldToHold();
}

std::ostream& Scheduler::dump(std::ostream& os) {
    os << "[Scheduler name=" << m_name
       << " size=" << m_threadCount
       << " active_count=" << m_activeThreadCount
       << " idle_count=" << m_idleThreadCount
       << " stopping=" << m_stopping
       << " ]" << std::endl << "    ";
    for(size_t i = 0; i < m_threadIds.size(); ++i) {
        if (i) {
            os << ", ";
        }
        os << m_threadIds[i];
    }
    return os;
}

SchedulerSwitcher::SchedulerSwitcher(Scheduler* target) {
    m_caller = Scheduler::GetThis();
    if(target) {
        target->switchTo();
    }
}

SchedulerSwitcher::~SchedulerSwitcher() {
    if(m_caller) {
        m_caller->switchTo();
    }
}

}