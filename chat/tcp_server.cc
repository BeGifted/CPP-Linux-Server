#include "tcp_server.h"
#include "config.h"
#include "log.h"

namespace chat {

static chat::ConfigVar<uint64_t>::ptr g_tcp_server_read_timeout =
    chat::Config::Lookup("tcp_server.read_timeout", (uint64_t)(60 * 1000 * 2), "tcp server read timeout");

static chat::Logger::ptr g_logger = CHAT_LOG_NAME("system");

TcpServer::TcpServer(chat::IOManager* worker,
                    chat::IOManager* io_worker,
                    chat::IOManager* accept_worker)
    :m_worker(worker)
    ,m_ioWorker(io_worker)
    ,m_acceptWorker(accept_worker)
    ,m_recvTimeout(g_tcp_server_read_timeout->getValue())
    ,m_name("chat/1.0.0")
    ,m_isStop(true) {
}

TcpServer::~TcpServer() {
    for (auto& i : m_socks) {
        i->close();
    }
    m_socks.clear();
}

void TcpServer::setConf(const TcpServerConf& v) {
    m_conf.reset(new TcpServerConf(v));
}

bool TcpServer::bind(chat::Address::ptr addr, bool ssl) {
    std::vector<Address::ptr> addrs;
    std::vector<Address::ptr> fails;
    addrs.push_back(addr);
    return bind(addrs, fails, ssl);
}

bool TcpServer::bind(const std::vector<Address::ptr>& addrs
                        ,std::vector<Address::ptr>& fails
                        ,bool ssl) {
    m_ssl = ssl;
    for(auto& addr : addrs) {
        Socket::ptr sock = ssl ? SSLSocket::CreateTCP(addr) : Socket::CreateTCP(addr);
        // Socket::ptr sock = Socket::CreateTCP(addr);
        if (!sock->bind(addr)) {
            CHAT_LOG_ERROR(g_logger) << "bind fail errno="
                << errno << " errstr=" << strerror(errno)
                << " addr=[" << addr->toString() << "]";
            fails.push_back(addr);
            continue;
        }
        if (!sock->listen()) {
            CHAT_LOG_ERROR(g_logger) << "listen fail errno="
                << errno << " errstr=" << strerror(errno)
                << " addr=[" << addr->toString() << "]";
            fails.push_back(addr);
            continue;
        }
        m_socks.push_back(sock);
    }

    if (!fails.empty()) {
        m_socks.clear();
        return false;
    }

    for(auto& i : m_socks) {
        CHAT_LOG_INFO(g_logger) << "type=" << m_type
            << " name=" << m_name
            << " ssl=" << m_ssl
            << " server bind success: " << i->toString();
    }
    return true;
}

void TcpServer::startAccept(Socket::ptr sock) {
    while(!m_isStop) {
        Socket::ptr client = sock->accept();
        if (client) {
            client->setRecvTimeout(m_recvTimeout);
            m_ioWorker->schedule(std::bind(&TcpServer::handleClient, shared_from_this(), client));
        } else {
            CHAT_LOG_ERROR(g_logger) << "accept errno=" << errno
                << " errstr=" << strerror(errno);
        }
    }
}

bool TcpServer::start() {
    if (!m_isStop) {
        return true;
    }
    m_isStop = false;
    for(auto& sock : m_socks) {
        m_acceptWorker->schedule(std::bind(&TcpServer::startAccept, shared_from_this(), sock));
    }
    return true;
}

void TcpServer::stop() {
    m_isStop = true;
    auto self = shared_from_this();
    m_acceptWorker->schedule([this, self]() {  //捕获了当前对象的成员变量（通过this捕获）和self指针（通过值捕获，确保在Lambda表达式执行期间保持对象的有效性）
        for(auto& sock : m_socks) {
            sock->cancelAll();
            sock->close();
        }
        m_socks.clear();
    });
}

void TcpServer::handleClient(Socket::ptr client) {
    CHAT_LOG_INFO(g_logger) << "handleClient: " << client->toString();
}

bool TcpServer::loadCertificates(const std::string& cert_file, const std::string& key_file) {
    for (auto& i : m_socks) {
        auto ssl_socket = std::dynamic_pointer_cast<SSLSocket>(i);
        if(ssl_socket) {
            if(!ssl_socket->loadCertificates(cert_file, key_file)) {
                return false;
            }
        }
    }
    return true;
}

std::string TcpServer::toString(const std::string& prefix) {
    std::stringstream ss;
    ss << prefix << "[type=" << m_type
       << " name=" << m_name << " ssl=" << m_ssl
       << " worker=" << (m_worker ? m_worker->getName() : "")
       << " accept=" << (m_acceptWorker ? m_acceptWorker->getName() : "")
       << " recv_timeout=" << m_recvTimeout << "]" << std::endl;
    std::string pfx = prefix.empty() ? "    " : prefix;
    for(auto& i : m_socks) {
        ss << pfx << pfx << i->toString() << std::endl;
    }
    return ss.str();
}

}