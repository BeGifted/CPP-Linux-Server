#ifndef __CHAT_APPLICATION_H__
#define __CHAT_APPLICATION_H__

#include "http/http_server.h"
#include "streams/service_discovery.h"
#include "grpc/grpc_loadbalance.h"

namespace chat {

class Application {
public:
    Application();

    static Application* GetInstance() { return s_instance;}
    bool init(int argc, char** argv);
    bool run();

    bool getServer(const std::string& type, std::vector<TcpServer::ptr>& svrs);
    void listAllServer(std::map<std::string, std::vector<TcpServer::ptr> >& servers);

    ZKServiceDiscovery::ptr getServiceDiscovery() const { return m_serviceDiscovery;}
    grpc::GrpcSDLoadBalance::ptr getGrpcSDLoadBalance() const { return m_grpcSDLoadBalance;}
    void initEnv();
private:
    int main(int argc, char** argv);
    int run_fiber();
private:
    int m_argc = 0;
    char** m_argv = nullptr;

    std::map<std::string, std::vector<TcpServer::ptr> > m_servers;
    IOManager::ptr m_mainIOManager;
    static Application* s_instance;

    ZKServiceDiscovery::ptr m_serviceDiscovery;
    grpc::GrpcSDLoadBalance::ptr m_grpcSDLoadBalance;
};

}

#endif