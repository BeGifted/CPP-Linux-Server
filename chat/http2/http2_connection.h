#ifndef __CHAT_HTTP2_HTTP2_CONNECTION_H__
#define __CHAT_HTTP2_HTTP2_CONNECTION_H__

#include "http2_socket_stream.h"

namespace chat {
namespace http2 {

class Http2Connection : public Http2SocketStream {
public:
    typedef std::shared_ptr<Http2Connection> ptr;
    Http2Connection();
    ~Http2Connection();

    bool connect(chat::Address::ptr addr, bool ssl = false);
    void reset();
protected:
    AsyncSocketStream::Ctx::ptr onStreamClose(Http2Stream::ptr stream) override;
    AsyncSocketStream::Ctx::ptr onHeaderEnd(Http2Stream::ptr stream) override;
};

}
}

#endif
