#ifndef __CHAT_DS_BLOCKING_QUEUE_H__
#define __CHAT_DS_BLOCKING_QUEUE_H__

#include "mutex.h"

namespace chat {
namespace ds {

template<class T>
class BlockingQueue {
public:
    typedef std::shared_ptr<BlockingQueue> ptr;
    typedef std::shared_ptr<T> data_type;
    typedef chat::Spinlock MutexType;

    size_t push(const data_type& data) {
        MutexType::Lock lock(m_mutex);
        m_datas.push_back(data);
        size_t size = m_datas.size();
        lock.unlock();
        m_sem.notify();
        return size;
    }

    data_type pop() {
        m_sem.wait();
        MutexType::Lock lock(m_mutex);
        auto v = m_datas.front();
        m_datas.pop_front();
        return v;
    }

    size_t size() {
        MutexType::Lock lock(m_mutex);
        return m_datas.size();
    }

    bool empty() {
        MutexType::Lock lock(m_mutex);
        return m_datas.empty();
    }

    void notifyAll() {
        m_sem.notifyAll();
    }
    
private:
    chat::FiberSemaphore m_sem;
    MutexType m_mutex;
    std::list<data_type> m_datas;
};

}
}

#endif