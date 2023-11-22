#ifndef __CHAT_WORKER_H__
#define __CHAT_WORKER_H__

#include "mutex.h"
#include "singleton.h"
#include "log.h"
#include "iomanager.h"

namespace chat {

class WorkerManager {
public:
    WorkerManager();
    void add(Scheduler::ptr s);
    Scheduler::ptr get(const std::string& name);
    IOManager::ptr getAsIOManager(const std::string& name);

    template<class FiberOrCb>
    void schedule(const std::string& name, FiberOrCb fc, int thread = -1) {
        auto s = get(name);
        if(s) {
            s->schedule(fc, thread);
        } else {
            static chat::Logger::ptr s_logger = CHAT_LOG_NAME("system");
            CHAT_LOG_ERROR(s_logger) << "schedule name=" << name << " not exists";
        }
    }

    template<class Iter>
    void schedule(const std::string& name, Iter begin, Iter end) {
        auto s = get(name);
        if(s) {
            s->schedule(begin, end);
        } else {
            static chat::Logger::ptr s_logger = CHAT_LOG_NAME("system");
            CHAT_LOG_ERROR(s_logger) << "schedule name=" << name
                << " not exists";
        }
    }

    bool init();
    bool init(const std::map<std::string, std::map<std::string, std::string> >& v);
    void stop();

    bool isStoped() const { return m_stop;}
    std::ostream& dump(std::ostream& os);

    uint32_t getCount();
private:
    std::map<std::string, std::vector<Scheduler::ptr> > m_datas;
    bool m_stop;
};

typedef chat::Singleton<WorkerManager> WorkerMgr;

}

#endif