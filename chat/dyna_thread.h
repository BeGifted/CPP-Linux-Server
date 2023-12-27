#ifndef __CHAT_DYNA_THREAD_H__
#define __CHAT_DYNA_THREAD_H__

#include <thread>
#include <vector>
#include <list>
#include <map>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>

#include "singleton.h"
#include "mutex.h"

namespace chat {

class DynaThread;
class IDynaThread {
public:
    typedef std::shared_ptr<IDynaThread> ptr;
    typedef std::function<void()> callback;

    virtual ~IDynaThread(){};
    virtual bool dispatch(callback cb) = 0;
    virtual bool dispatch(uint32_t id, callback cb) = 0;
    virtual bool batchDispatch(const std::vector<callback>& cbs) = 0;
    virtual void broadcast(callback cb) = 0;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void join() = 0;
    virtual void dump(std::ostream& os) = 0;
    virtual uint64_t getTotal() = 0;
};

class DynaThread : public IDynaThread {
public:
    typedef std::shared_ptr<DynaThread> ptr;
    typedef IDynaThread::callback callback;
    typedef std::function<void (DynaThread*)> init_cb;
    DynaThread(const std::string& name = "", struct event_base* base = NULL);
    ~DynaThread();

    static DynaThread* GetThis();
    static const std::string& GetDynaThreadName();
    static void GetAllDynaThreadName(std::map<uint64_t, std::string>& names);

    void setThis();
    void unsetThis();

    void start();

    virtual bool dispatch(callback cb);
    virtual bool dispatch(uint32_t id, callback cb);
    virtual bool batchDispatch(const std::vector<callback>& cbs);
    virtual void broadcast(callback cb);

    void join();
    void stop();
    bool isStart() const { return m_start;}

    struct event_base* getBase() { return m_base;}
    std::thread::id getId() const;

    void* getData(const std::string& name);
    template<class T>
    T* getData(const std::string& name) {
        return (T*)getData(name);
    }
    void setData(const std::string& name, void* v);

    void setInitCb(init_cb v) { m_initCb = v;}

    void dump(std::ostream& os);
    virtual uint64_t getTotal() { return m_total;}
private:
    void thread_cb();
    static void read_cb(evutil_socket_t sock, short which, void* args);
private:
    evutil_socket_t m_read;
    evutil_socket_t m_write;
    struct event_base* m_base;
    struct event* m_event;
    std::thread* m_thread;
    chat::RWMutex m_mutex;
    std::list<callback> m_callbacks;

    std::string m_name;
    init_cb m_initCb;

    std::map<std::string, void*> m_datas;

    bool m_working;
    bool m_start;
    uint64_t m_total;
};

class DynaThreadPool : public IDynaThread {
public:
    typedef std::shared_ptr<DynaThreadPool> ptr;
    typedef IDynaThread::callback callback;

    DynaThreadPool(uint32_t size, const std::string& name = "", bool advance = false);
    ~DynaThreadPool();

    void start();
    void stop();
    void join();

    //随机线程执行
    bool dispatch(callback cb);
    bool batchDispatch(const std::vector<callback>& cb);
    //指定线程执行
    bool dispatch(uint32_t id, callback cb);

    DynaThread* getRandDynaThread();
    void setInitCb(DynaThread::init_cb v) { m_initCb = v;}

    void dump(std::ostream& os);

    void broadcast(callback cb);
    virtual uint64_t getTotal() { return m_total;}
private:
    void releaseDynaThread(DynaThread* t);
    void check();

    void wrapcb(std::shared_ptr<DynaThread>, callback cb);
private:
    uint32_t m_size;
    uint32_t m_cur;
    std::string m_name;
    bool m_advance;
    bool m_start;
    RWMutex m_mutex;
    std::list<callback> m_callbacks;
    std::vector<DynaThread*> m_threads;
    std::list<DynaThread*> m_freeDynaThreads;
    DynaThread::init_cb m_initCb;
    uint64_t m_total;
};

class DynaThreadManager {
public:
    typedef IDynaThread::callback callback;
    void dispatch(const std::string& name, callback cb);
    void dispatch(const std::string& name, uint32_t id, callback cb);
    void batchDispatch(const std::string& name, const std::vector<callback>& cbs);
    void broadcast(const std::string& name, callback cb);

    void dumpDynaThreadStatus(std::ostream& os);

    void init();
    void start();
    void stop();

    IDynaThread::ptr get(const std::string& name);
    void add(const std::string& name, IDynaThread::ptr thr);
private:
    std::map<std::string, IDynaThread::ptr> m_threads;
};

typedef Singleton<DynaThreadManager> DynaThreadMgr;

}
#endif