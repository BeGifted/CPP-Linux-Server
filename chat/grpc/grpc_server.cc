#include "grpc_server.h"
#include "chat/log.h"
#include "chat/grpc/grpc_servlet.h"

namespace chat {
namespace grpc {

static chat::Logger::ptr g_logger = CHAT_LOG_NAME("system");

GrpcServer::GrpcServer(chat::IOManager* worker
                        ,chat::IOManager* io_worker
                        ,chat::IOManager* accept_worker)
    :http2::Http2Server(worker, io_worker, accept_worker) {
    m_type = "grpc";
}

bool GrpcServer::isStreamPath(const std::string& path) {
    CHAT_LOG_INFO(g_logger) << "size: " << m_streamTypes.size();
    for(auto& i : m_streamTypes) {
        CHAT_LOG_INFO(g_logger) << "<<<< " << i.first << " : " << i.second;
    }
    return getStreamPathType(path) > 0;
}

bool GrpcServer::needSendResponse(const std::string& path) {
    uint32_t type = getStreamPathType(path);
    return type == 0 || type == (uint32_t)grpc::GrpcType::UNARY
        || type == (uint32_t)grpc::GrpcType::CLIENT;
}

uint32_t GrpcServer::getStreamPathType(const std::string& path) {
    chat::RWMutex::ReadLock lock(m_mutex);
    auto it = m_streamTypes.find(path);
    if(it == m_streamTypes.end()) {
        return 0;
    }
    return it->second;
}

void GrpcServer::addStreamPath(const std::string& path, uint32_t type) {
    chat::RWMutex::WriteLock lock(m_mutex);
    m_streamTypes[path] = type;
}

void GrpcServer::handleClient(Socket::ptr client) {
    CHAT_LOG_INFO(g_logger) << "**** handleClient " << *client;
    chat::grpc::GrpcSession::ptr session = std::make_shared<chat::grpc::GrpcSession>(client, this);
    if(!session->handleShakeServer()) {
        CHAT_LOG_WARN(g_logger) << "grpc session handleShake fail, " << session->getRemoteAddressString();
        session->close();
        return;
    }
    session->setWorker(m_worker);
    session->start();
}

void GrpcServer::addGrpcServlet(const std::string& path, GrpcServlet::ptr slt) {
    m_dispatch->addServletCreator(path, std::make_shared<CloneServletCreator>(slt));
    addStreamPath(path, (uint8_t)slt->getType());
}

}
}
