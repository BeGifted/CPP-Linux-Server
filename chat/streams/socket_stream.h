#ifndef __CHAT_SOCKET_STREAM_H__
#define __CHAT_SOCKET_STREAM_H__

#include "chat/stream.h"
#include "chat/socket.h"
#include "chat/mutex.h"
#include "chat/iomanager.h"

namespace chat {


class SocketStream : public Stream {
public:
    typedef std::shared_ptr<SocketStream> ptr;

    SocketStream(Socket::ptr sock, bool owner = true);
    ~SocketStream();

    virtual int read(void* buffer, size_t length) override;
    virtual int read(ByteArray::ptr ba, size_t length) override;

    virtual int write(const void* buffer, size_t length) override;
    virtual int write(ByteArray::ptr ba, size_t length) override;

    virtual void close() override;

    Socket::ptr getSocket() const { return m_socket;}

    bool isConnected() const;
    bool checkConnected();

    Address::ptr getRemoteAddress();
    Address::ptr getLocalAddress();
    std::string getRemoteAddressString();
    std::string getLocalAddressString();

    uint64_t getId() const { return m_id;}
    
protected:
    Socket::ptr m_socket;
    uint64_t m_id: 63;
    //是否主控
    bool m_owner: 1;
};

}

#endif