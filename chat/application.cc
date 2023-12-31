#include "application.h"
#include <unistd.h>
#include <signal.h>
#include "tcp_server.h"
#include "daemon.h"
#include "config.h"
#include "env.h"
#include "log.h"
#include "worker.h"
#include "http/ws_server.h"
#include "http/http_server.h"
#include "util.h"
#include "http2/http2_server.h"
#include "grpc/grpc_server.h"


namespace chat {

static chat::Logger::ptr g_logger = CHAT_LOG_NAME("system");

static chat::ConfigVar<std::string>::ptr g_server_work_path =
    chat::Config::Lookup("server.work_path"
            ,std::string("/apps/work/chat")
            , "server work path");

static chat::ConfigVar<std::string>::ptr g_server_pid_file =
    chat::Config::Lookup("server.pid_file"
            ,std::string("chat.pid")
            , "server pid file");

static chat::ConfigVar<std::vector<TcpServerConf> >::ptr g_servers_conf
    = chat::Config::Lookup("servers", std::vector<TcpServerConf>(), "http server config");

static chat::ConfigVar<std::string>::ptr g_service_discovery_zk =
    chat::Config::Lookup("service_discovery.zk"
            ,std::string("")
            , "service discovery zookeeper");

Application* Application::s_instance = nullptr;

Application::Application() {
    s_instance = this;
}

bool Application::init(int argc, char** argv) {
    m_argc = argc;
    m_argv = argv;

    chat::EnvMgr::GetInstance()->addHelp("s", "start with the terminal");
    chat::EnvMgr::GetInstance()->addHelp("d", "run as daemon");
    chat::EnvMgr::GetInstance()->addHelp("c", "conf path default: ./conf");
    chat::EnvMgr::GetInstance()->addHelp("p", "print help");

    bool is_print_help = false;
    if (!chat::EnvMgr::GetInstance()->init(argc, argv)) {
        is_print_help = true;
    }

    if (chat::EnvMgr::GetInstance()->has("p")) {
        is_print_help = true;
    }

    std::string conf_path = chat::EnvMgr::GetInstance()->getConfigPath();
    CHAT_LOG_INFO(g_logger) << "load conf path:" << conf_path;
    chat::Config::LoadFromConfDir(conf_path);

    // ModuleMgr::GetInstance()->init();
    // std::vector<Module::ptr> modules;
    // ModuleMgr::GetInstance()->listAll(modules);

    // for(auto i : modules) {
    //     i->onBeforeArgsParse(argc, argv);
    // }

    if (is_print_help) {
        chat::EnvMgr::GetInstance()->printHelp();
        return false;
    }

    // for(auto i : modules) {
    //     i->onAfterArgsParse(argc, argv);
    // }
    // modules.clear();

    int run_type = 0;
    if (chat::EnvMgr::GetInstance()->has("s")) {
        run_type = 1;
    }
    if (chat::EnvMgr::GetInstance()->has("d")) {
        run_type = 2;
    }

    if (run_type == 0) {
        chat::EnvMgr::GetInstance()->printHelp();
        return false;
    }

    std::string pidfile = g_server_work_path->getValue() + "/" + g_server_pid_file->getValue();
    if (chat::FSUtil::IsRunningPidfile(pidfile)) {
        CHAT_LOG_ERROR(g_logger) << "server is running:" << pidfile;
        return false;
    }

    if (!chat::FSUtil::Mkdir(g_server_work_path->getValue())) {
        CHAT_LOG_FATAL(g_logger) << "create work path [" << g_server_work_path->getValue()
            << " errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

bool Application::run() {
    bool is_daemon = chat::EnvMgr::GetInstance()->has("d");
    return start_daemon(m_argc, m_argv, std::bind(&Application::main, this, std::placeholders::_1,
                                                  std::placeholders::_2), is_daemon);
}

void Application::initEnv() {
    chat::WorkerMgr::GetInstance()->init();

    if(!g_service_discovery_zk->getValue().empty()) {
        m_serviceDiscovery = std::make_shared<ZKServiceDiscovery>(g_service_discovery_zk->getValue());
    }
}

void sigproc(int sig) {
    CHAT_LOG_INFO(g_logger) << "sigproc sig=" << sig;
    if(sig == SIGUSR1) {
        chat::LoggerMgr::GetInstance()->reopen();
    }
}

int Application::main(int argc, char** argv) {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR1, sigproc);

    CHAT_LOG_INFO(g_logger) << "main";
    std::string conf_path = chat::EnvMgr::GetInstance()->getConfigPath();
    chat::Config::LoadFromConfDir(conf_path, true);
    {
        std::string pidfile = g_server_work_path->getValue() + "/" + g_server_pid_file->getValue();
        std::ofstream ofs(pidfile);
        if(!ofs) {
            CHAT_LOG_ERROR(g_logger) << "open pidfile " << pidfile << " failed";
            return false;
        }
        ofs << getpid();
    }


    m_mainIOManager = std::make_shared<chat::IOManager>(1, true, "main");
    m_mainIOManager->schedule(std::bind(&Application::run_fiber, this));
    m_mainIOManager->addTimer(2000, [conf_path](){
        chat::Config::LoadFromConfDir(conf_path);
    }, true);
    m_mainIOManager->stop();
    return 0;
}

int Application::run_fiber() {
    // std::vector<Module::ptr> modules;
    // ModuleMgr::GetInstance()->listAll(modules);
    // bool has_error = false;
    // for(auto& i : modules) {
    //     if(!i->onLoad()) {
    //         CHAT_LOG_ERROR(g_logger) << "module name="
    //             << i->getName() << " version=" << i->getVersion()
    //             << " filename=" << i->getFilename();
    //         has_error = true;
    //     }
    // }
    // if(has_error) {
    //     _exit(0);
    // }

    chat::WorkerMgr::GetInstance()->init();
    // FoxThreadMgr::GetInstance()->init();
    // FoxThreadMgr::GetInstance()->start();
    // RedisMgr::GetInstance();

    auto http_confs = g_servers_conf->getValue();
    std::vector<TcpServer::ptr> svrs;
    for(auto& i : http_confs) {
        CHAT_LOG_DEBUG(g_logger) << std::endl << LexicalCast<TcpServerConf, std::string>()(i);

        std::vector<Address::ptr> address;
        for(auto& a : i.address) {
            size_t pos = a.find(":");
            if (pos == std::string::npos) {
                address.push_back(std::make_shared<UnixAddress>(a));
                continue;
            }
            int32_t port = atoi(a.substr(pos + 1).c_str());
            auto addr = chat::IPAddress::Create(a.substr(0, pos).c_str(), port);
            if (addr) {
                address.push_back(addr);
                continue;
            }
            std::vector<std::pair<Address::ptr, uint32_t> > result;
            if (chat::Address::GetInterfaceAddresses(result, a.substr(0, pos))) {
                for(auto& x : result) {
                    auto ipaddr = std::dynamic_pointer_cast<IPAddress>(x.first);
                    if (ipaddr) {
                        ipaddr->setPort(atoi(a.substr(pos + 1).c_str()));
                    }
                    address.push_back(ipaddr);
                }
                continue;
            }

            auto aaddr = chat::Address::LookupAny(a);  //host
            if (aaddr) {
                address.push_back(aaddr);
                continue;
            }
            CHAT_LOG_ERROR(g_logger) << "invalid address: " << a;
            _exit(0);
        }
        IOManager* accept_worker = chat::IOManager::GetThis();
        IOManager* io_worker = chat::IOManager::GetThis();
        IOManager* process_worker = chat::IOManager::GetThis();
        if (!i.accept_worker.empty()) {
            accept_worker = chat::WorkerMgr::GetInstance()->getAsIOManager(i.accept_worker).get();
            if (!accept_worker) {
                CHAT_LOG_ERROR(g_logger) << "accept_worker: " << i.accept_worker << " not exists";
                _exit(0);
            }
        }
        if (!i.io_worker.empty()) {
            io_worker = chat::WorkerMgr::GetInstance()->getAsIOManager(i.io_worker).get();
            if (!io_worker) {
                CHAT_LOG_ERROR(g_logger) << "io_worker: " << i.io_worker << " not exists";
                _exit(0);
            }
        }
        if (!i.process_worker.empty()) {
            process_worker = chat::WorkerMgr::GetInstance()->getAsIOManager(i.process_worker).get();
            if (!process_worker) {
                CHAT_LOG_ERROR(g_logger) << "process_worker: " << i.process_worker << " not exists";
                _exit(0);
            }
        }

        TcpServer::ptr server;
        if(i.type == "http") {
            server = std::make_shared<chat::http::HttpServer>(i.keepalive, process_worker, io_worker, accept_worker);
        } else if(i.type == "http2") {
            server = std::make_shared<chat::http2::Http2Server>(process_worker, io_worker, accept_worker); 
        } else if(i.type == "grpc") {
            server = std::make_shared<chat::grpc::GrpcServer>(process_worker, io_worker, accept_worker);
        } else if(i.type == "ws") {
            server = std::make_shared<chat::http::WSServer>(process_worker, io_worker, accept_worker);
        } else if(i.type == "rock") {
            // server.reset(new chat::RockServer("rock", process_worker, io_worker, accept_worker));
        } else if(i.type == "nameserver") {
            // server.reset(new chat::RockServer("nameserver", process_worker, io_worker, accept_worker));
            // ModuleMgr::GetInstance()->add(std::make_shared<chat::ns::NameServerModule>());
        } else {
            CHAT_LOG_ERROR(g_logger) << "invalid server type=" << i.type << LexicalCast<TcpServerConf, std::string>()(i);
            _exit(0);
        }
        if (!i.name.empty()) {
            server->setName(i.name);
        }
        std::vector<Address::ptr> fails;
        if (!server->bind(address, fails, i.ssl)) {
            for(auto& x : fails) {
                CHAT_LOG_ERROR(g_logger) << "bind address fail:" << *x;
            }
            _exit(0);
        }
        if (i.ssl) {
            if (!server->loadCertificates(i.cert_file, i.key_file)) {
                CHAT_LOG_ERROR(g_logger) << "loadCertificates fail, cert_file=" << i.cert_file << " key_file=" << i.key_file;
            }
        }
        server->setConf(i);
        m_servers[i.type].push_back(server);
        svrs.push_back(server);
    }

    if(!g_service_discovery_zk->getValue().empty()) {
        m_serviceDiscovery = std::make_shared<ZKServiceDiscovery>(g_service_discovery_zk->getValue());
        m_grpcSDLoadBalance = std::make_shared<grpc::GrpcSDLoadBalance>(m_serviceDiscovery);
    }

    if(m_grpcSDLoadBalance) {
        m_grpcSDLoadBalance->start();
        sleep(1);
    }

    // for(auto& i : modules) {
    //     i->onServerReady();
    // }

    for(auto& i : svrs) {
        i->start();
    }

    // for(auto& i : modules) {
    //     i->onServerUp();
    // }

    if(m_grpcSDLoadBalance) {
        m_grpcSDLoadBalance->doRegister();
    }

    // ZKServiceDiscovery::ptr m_serviceDiscovery;
    //RockSDLoadBalance::ptr m_rockSDLoadBalance;
    //chat::ZKServiceDiscovery::ptr zksd(new chat::ZKServiceDiscovery("127.0.0.1:21811"));
    //zksd->registerServer("blog", "chat", chat::GetIPv4() + ":8090", "xxx");
    //zksd->queryServer("blog", "chat");
    //zksd->setSelfInfo(chat::GetIPv4() + ":8090");
    //zksd->setSelfData("vvv");
    //static RockSDLoadBalance::ptr rsdlb(new RockSDLoadBalance(zksd));
    //rsdlb->start();
    return 0;
}

bool Application::getServer(const std::string& type, std::vector<TcpServer::ptr>& svrs) {
    auto it = m_servers.find(type);
    if(it == m_servers.end()) {
        return false;
    }
    svrs = it->second;
    return true;
}

void Application::listAllServer(std::map<std::string, std::vector<TcpServer::ptr> >& servers) {
    servers = m_servers;
}

}