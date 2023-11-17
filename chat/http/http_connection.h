#ifndef __CHAT_HTTP_CONNECTION_H__
#define __CHAT_HTTP_CONNECTION_H__

#include "chat/streams/socket_stream.h"
#include "http.h"

namespace chat{
namespace http{

class HttpConnection: public SocketStream{
public:
    typedef std::shared_ptr<HttpConnection> ptr;

    HttpConnection(Socket::ptr sock, bool owner = true);

    HttpResponse::ptr recvResponse();
    int sendRequest(HttpRequest::ptr rsp);

};

}
}



#endif