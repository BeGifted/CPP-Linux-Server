#ifndef __CHAT_IOMANAGER_H__
#define __CHAT_IOMANAGER_H__

#include "scheduler.h"
#include "timer.h"

namespace chat {

class IOManager: public Scheduler, public TimerManager {
public:
    typedef std::shared_ptr<IOManager> ptr;
    typedef RWMutex RWMutexType;

    enum Event {
        NONE = 0x0,
        READ = 0x1,  //=epollin
        WRITE = 0x4  //=epollout
    };

private:
    struct FdContext {
        typedef Mutex MutexType;
        struct EventContext {
            Scheduler* scheduler = nullptr;  //事件执行
            Fiber::ptr fiber;  //事件协程
            std::function<void()> cb;  //事件回调
        };

        EventContext& getContext(Event evnet);
        void resetContext(EventContext& ctx);
        void triggerEvent(Event event);

        int fd = 0;  //事件关联句柄
        EventContext read;
        EventContext write;
        Event m_events = Event::NONE;  //已注册事件
        MutexType mutex;
    };
public:
    IOManager(size_t threads = 1, bool use_caller = true, const std::string& name = "");
    ~IOManager();

    int addEvent(int fd, Event event, std::function<void()> cb = nullptr);
    bool delEvent(int fd, Event event);
    bool cancelEvent(int fd, Event event);
    bool cancelAll(int fd);

    static IOManager* GetThis();

protected:
    void tickle() override;
    bool stopping() override;
    void idle() override;
    void onTimerInsertedAtFront() override;

    void contextResize(size_t size);
    bool stopping(uint64_t& timeout);

private:
    int m_epfd = 0;
    int m_tickleFds[2];

    std::atomic<size_t> m_pendingEventCount = {0}; //等待执行的事件数量
    RWMutexType m_mutex;
    std::vector<FdContext*> m_fdcontexts;

};

}

#endif