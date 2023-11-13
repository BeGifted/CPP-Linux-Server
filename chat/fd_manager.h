#ifndef __FD_MANAGER_H__
#define __FD_MANAGER_H__

#include <memory>
#include "thread.h"
#include "singleton.h"

namespace chat {

class FdCtx: public std::enable_shared_from_this<FdCtx> {
public: 
    typedef std::shared_ptr<FdCtx> ptr;
    FdCtx(int fd);
    ~FdCtx();

    bool init();
    bool isInit() const { return m_isInit; }
    bool isSocket() const { return m_isSocket; }
    bool isClosed() const { return m_isClosed; }
    bool close();

    void setUserNonblock(bool v) { m_userNonblock = v; }
    bool getUserNonblock() { return m_userNonblock; }

    void setSysNonblock(bool v) { m_userNonblock = v; }
    bool getSysNonblock() { return m_userNonblock; }

    void setTimeout(int type, uint64_t v);
    uint64_t getTimeout(int type);
private:
    //使用位域的优点是在某些情况下可以节省内存空间，因为布尔类型通常占用1字节，而使用位域可以将多个布尔成员放在一个字节中，从而减少内存使用。
    bool m_isInit: 1;
    bool m_isSocket: 1;
    bool m_sysNonblock: 1;
    bool m_isClosed: 1;
    bool m_userNonblock: 1;
    int m_fd;

    uint64_t m_recvTimeout;
    uint64_t m_sendTimeout;
};

class FdManager{
public:
    typedef RWMutex RWMutexType;

    FdManager();
    FdCtx::ptr get(int fd, bool auto_create = false);
    void del(int fd);

private:
    RWMutexType m_mutex;
    std::vector<FdCtx::ptr> m_datas;
};

typedef Singleton<FdManager> FdMgr;
}

#endif