#ifndef __CHAT_GRPC_GRPC_SERVER_H__
#define __CHAT_GRPC_GRPC_SERVER_H__

#include "chat/http2/http2_server.h"
#include "grpc_servlet.h"

namespace chat {
namespace grpc {

class GrpcServer : public http2::Http2Server {
public:
    typedef std::shared_ptr<GrpcServer> ptr;
    GrpcServer(chat::IOManager* worker = chat::IOManager::GetThis()
                ,chat::IOManager* io_worker = chat::IOManager::GetThis()
                ,chat::IOManager* accept_worker = chat::IOManager::GetThis());

    bool isStreamPath(const std::string& path);
    bool needSendResponse(const std::string& path);
    uint32_t getStreamPathType(const std::string& path);

    void addGrpcServlet(const std::string& path, GrpcServlet::ptr slt);
protected:
    void addStreamPath(const std::string& path, uint32_t type);
    virtual void handleClient(Socket::ptr client) override;
private:
    chat::RWMutex m_mutex;
    std::unordered_map<std::string, uint32_t> m_streamTypes;
};

}
}

#endif
