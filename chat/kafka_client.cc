#include "kafka_client.h"
#include "log.h"
#include "util.h"
#include "config.h"
#include "dyna_thread.h"

namespace chat {

static chat::Logger::ptr g_logger = CHAT_LOG_NAME("system");

class MyEventCb : public RdKafka::EventCb {
public:
    MyEventCb(bool& conn)
        :m_connected(conn) {
    }
    virtual ~MyEventCb() {}

    void event_cb (RdKafka::Event &event) {
        switch (event.type()) {
            case RdKafka::Event::EVENT_ERROR:
                CHAT_LOG_ERROR(g_logger) << "Cb:ERROR (" << RdKafka::err2str(event.err())
                            << "): " << event.str();
                if (event.err() == RdKafka::ERR__ALL_BROKERS_DOWN) {
                    m_connected = false;
                }
                break;
            case RdKafka::Event::EVENT_STATS:
                CHAT_LOG_ERROR(g_logger) << "Cb:\"STATS\": " << event.str();
                break;
            case RdKafka::Event::EVENT_LOG:
                CHAT_LOG_ERROR(g_logger) << "Cb:LOG-" << event.severity() << "-" << event.fac()
                            << ": " << event.str();
                break;
            default:
                CHAT_LOG_ERROR(g_logger) << "Cb:EVENT " << event.type()
                            << " (" << RdKafka::err2str(event.err()) << "): "
                            << event.str() << std::endl;
            break;
        }
    }

private:
    bool& m_connected;
};

class MyDeliveryReportCb : public RdKafka::DeliveryReportCb {
public:
    MyDeliveryReportCb(bool& conn, uint64_t& c)
        :m_connected(conn)
        ,m_total(c) {
    }
    virtual ~MyDeliveryReportCb() {}
    void dr_cb (RdKafka::Message &message) {
        if(message.err()) {
            CHAT_LOG_ERROR(g_logger) << "Message delivery for (" << message.len()
                        << " bytes): " << message.errstr()
                        << (message.key() ? (" Key: " + *(message.key())) : "");
            m_connected = false;
        } else {
            chat::Atomic::addFetch(m_total, 1);
            m_connected = true;
        }
    }
private:
    bool& m_connected;
    uint64_t& m_total;
};

class MyConsumerCb : public RdKafka::ConsumeCb {
public:
    MyConsumerCb(KafkaConsumer* c) {
        m_consumer = c;
        m_cb = c->getCallback();
    }
    void consume_cb(RdKafka::Message& msg, void* opaque) {
        m_cb(msg, m_consumer);
        m_consumer->updateOffset(msg);
    }

private:
    KafkaConsumer* m_consumer;
    KafkaConsumer::message_cb m_cb;
};

void metadata_print(const std::string& topic,
                    const RdKafka::Metadata* metadata) {
    static rd_kafka_metadata_t a;
    a.broker_cnt = a.broker_cnt;
}

KafkaTopic::KafkaTopic(topic_ptr t)
    :m_topic(t)
    ,m_count(0) {
}

uint64_t KafkaTopic::incCount() {
    return chat::Atomic::addFetch(m_count, 1);
}

KafkaProducer::KafkaProducer()
    :m_connected(false)
    ,m_eventCb(NULL)
    ,m_drCb(NULL)
    ,m_totalMsg(0)
    ,m_sucessMsg(0) {

    m_conf.reset(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
    m_tconf.reset(RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC));

    m_eventCb = new MyEventCb(m_connected);
    m_conf->set("event_cb", m_eventCb, m_err);

    m_drCb = new MyDeliveryReportCb(m_connected, m_sucessMsg);
    m_conf->set("dr_cb", m_drCb, m_err);
}

KafkaProducer::~KafkaProducer() {
    if(m_eventCb) {
        delete m_eventCb;
    }
    if(m_drCb) {
        delete m_drCb;
    }
}

bool KafkaProducer::setBrokerList(const std::string& bl) {
    m_brokerList = bl;
    auto rt = m_conf->set("metadata.broker.list", bl, m_err);
    if(rt != RdKafka::Conf::CONF_OK) {
        CHAT_LOG_ERROR(g_logger) << "set brokerlist fail: " << bl << " " << rt << ":" << m_err;
        return false;
    }
    m_producer.reset(RdKafka::Producer::create(m_conf.get(), m_err));
    if(!m_producer) {
        CHAT_LOG_ERROR(g_logger) << "create producer fail: " << m_err;
        return false;
    }
    return true;
}

KafkaTopic::ptr KafkaProducer::getTopic(const std::string& topic) {
    do {
        chat::RWMutex::ReadLock lock(m_mutex);
        auto it = m_topics.find(topic);
        if(it != m_topics.end()) {
            return it->second;
        }
    } while(0);
    chat::RWMutex::WriteLock lock(m_mutex);
    auto it = m_topics.find(topic);
    if(it != m_topics.end()) {
        return it->second;
    }
    KafkaTopic::topic_ptr t(RdKafka::Topic::create(m_producer.get(), topic, m_tconf.get(), m_err));
    if(!t) {
        CHAT_LOG_ERROR(g_logger) << "create topic fail: " << topic << " " << m_err;
        return nullptr;
    }
    KafkaTopic::ptr tt(new KafkaTopic(t));
    m_topics[topic] = tt;
    return tt;
}

bool KafkaProducer::produce(const std::string& topic, const std::string& msg) {
    KafkaTopic::ptr t = getTopic(topic);
    if(!t) {
        return false;
    }
    chat::Atomic::addFetch(m_totalMsg, 1);

    t->incCount();
    RdKafka::ErrorCode resp = m_producer->produce(t->getTopic().get(), RdKafka::Topic::PARTITION_UA,
            RdKafka::Producer::RK_MSG_COPY, const_cast<char* >(msg.c_str()),
            msg.size(), NULL, NULL);
    if(resp != RdKafka::ERR_NO_ERROR) {
        CHAT_LOG_ERROR(g_logger) << "Produce failed: " << RdKafka::err2str(resp);
    } else {
        CHAT_LOG_DEBUG(g_logger) << "Produced message (" << msg.size() << ") " << msg;
    }
    m_producer->poll(0); //poll()参数为0意味着不阻塞，主要是为了触发应用程序提供的回调函数
    return true;
}

bool KafkaProducer::produce(const std::string& topic, const std::string& msg, const std::string& key) {
    KafkaTopic::ptr t = getTopic(topic);
    if(!t) {
        return false;
    }
    t->incCount();
    chat::Atomic::addFetch(m_totalMsg, 1);
    RdKafka::ErrorCode resp = m_producer->produce(t->getTopic().get(), RdKafka::Topic::PARTITION_UA,
            RdKafka::Producer::RK_MSG_COPY, const_cast<char* >(msg.c_str()),
            msg.size(), &key, NULL);
    if(resp != RdKafka::ERR_NO_ERROR) {
        CHAT_LOG_ERROR(g_logger) << "Produce failed: " << RdKafka::err2str(resp);
    } else {
        CHAT_LOG_DEBUG(g_logger) << "Produced message (" << msg.size() << ") " << msg;
    }
    m_producer->poll(0);
    return true;
}

void KafkaProducer::poll(uint64_t t) {
    m_producer->poll(t);
}

void KafkaProducer::dump(std::ostream& os) {
    os << "KafkaProducer broker_list=" << m_brokerList << std::endl;
    os << "    total: " << m_totalMsg << std::endl;
    os << "    success: " << m_sucessMsg << std::endl;

    chat::RWMutex::ReadLock lock(m_mutex);
    for(auto& i : m_topics) {
        os << "    topic." << i.first << ": " << i.second->getCount() << std::endl;
    }
}

void KafkaProducer::listTopics(std::map<std::string, uint64_t>& topics) {
    chat::RWMutex::ReadLock lock(m_mutex);
    for(auto& i : m_topics) {
        topics.insert(std::make_pair(i.first, i.second->getCount()));
    }
}

KafkaProducerGroup::KafkaProducerGroup()
    :m_size(1)
    ,m_index(0) {
}

KafkaProducerGroup::~KafkaProducerGroup() {
    for(auto& i : m_mutexs) {
        if(i) {
            delete i;
        }
    }
}

void KafkaProducerGroup::start() {
    for(int32_t i = 0; i < m_size; ++i) {
        KafkaProducer::ptr producer = std::make_shared<KafkaProducer>();
        producer->setBrokerList(m_brokerList);
        m_producers.push_back(producer);
        m_mutexs.push_back(new chat::Spinlock);
    }
    m_datas.resize(m_size);
    for(int32_t i = 0; i < m_size; ++i) {
        chat::DynaThreadMgr::GetInstance()->dispatch(m_threadName, [this, i](){
            auto producer = m_producers[i];
            auto& data = m_datas[i];
            auto& mutex = *m_mutexs[i];
            uint64_t sleep_us = 0;
            while(true) {
                std::list<Msg::ptr> tmp;
                chat::Spinlock::Lock lock(mutex);
                std::swap(tmp, data);
                lock.unlock();

                if(tmp.empty()) {
                    if(sleep_us < 1000 * 15) {
                        sleep_us += 500;
                    }
                    usleep(sleep_us);
                    continue;
                }
                sleep_us = 0;

                for(auto& i : tmp) {
                    if(i->key.empty()) {
                        producer->produce(i->topic, i->msg);
                    } else {
                        producer->produce(i->topic, i->msg, i->key);
                    }
                }
            }
        });
    }
}

bool KafkaProducerGroup::produce(const std::string& topic, const std::string& msg, const std::string& key) {
    auto idx = chat::Atomic::addFetch(m_index, 1) % m_size;
    Msg::ptr m = std::make_shared<Msg>(topic, msg, key);

    chat::Spinlock::Lock lock(*m_mutexs[idx]);
    m_datas[idx].push_back(m);

    return true;
}

std::ostream& KafkaProducerGroup::dump(std::ostream& os) {
    os << "[KafkaProducerGroup size=" << m_size
       << " thread_name=" << m_threadName
       << " broker_list=["
       << m_brokerList << "]]";
    for(auto& i : m_producers) {
        if(i) {
            os << "***********************************************************" << std::endl;
            i->dump(os);
        }
    }
    return os;
}


KafkaConsumer::KafkaConsumer()
    :m_connected(false)
    ,m_running(false)
    ,m_ccb(true)
    ,m_eventCb(NULL)
    ,m_totalMsg(0)
    ,m_sucessMsg(0)
    ,m_partition(0)
    ,m_startOffset(0)
    ,m_curOffset(0) {

    m_conf.reset(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));

    m_eventCb = new MyEventCb(m_connected);
    m_conf->set("event_cb", m_eventCb, m_err);
}

KafkaConsumer::~KafkaConsumer() {
    if(m_eventCb) {
        delete m_eventCb;
    }
}

bool KafkaConsumer::setBrokerList(const std::string& bl) {
    m_brokerList = bl;
    auto rt = m_conf->set("metadata.broker.list", bl, m_err);
    if(rt != RdKafka::Conf::CONF_OK) {
        CHAT_LOG_ERROR(g_logger) << "set brokerlist fail: " << bl << " " << rt << ":" << m_err;
        return false;
    }
    m_consumer.reset(RdKafka::Consumer::create(m_conf.get(), m_err));
    if(!m_consumer) {
        CHAT_LOG_ERROR(g_logger) << "create consumer fail: " << m_err;
        return false;
    }
    return true;
}


void KafkaConsumer::poll(uint64_t t) {
    m_consumer->poll(t);
}

void KafkaConsumer::start(const std::string& topic_name,
                          int32_t partition,
                          int64_t start_offset,
                          const std::string& offset_path) {
    //if(m_running) {
    //    return;
    //}
    m_running = true;

    conf_ptr tconf(RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC));
    if(!offset_path.empty()) {
        tconf->set("offset.store.path", offset_path, m_err);
        tconf->set("offset.store.method", "file", m_err);
    }
    KafkaTopic::topic_ptr tc(RdKafka::Topic::create(m_consumer.get(), topic_name, tconf.get(), m_err));
    if(!tc) {
        CHAT_LOG_ERROR(g_logger) << "create topic fail: " << topic_name << " " << m_err;
        return;
    }
    KafkaTopic::ptr tpc(new KafkaTopic(tc));
    m_topic = tpc;

    {
        chat::RWMutex::WriteLock lock(m_mutex);
        m_partitionStartOffset[partition] = start_offset;
    }

    m_partition = partition;
    m_startOffset = start_offset;

    MyConsumerCb cb(this);
    auto t = tc.get();
    do {
        RdKafka::ErrorCode resp = m_consumer->start(t, partition, m_startOffset);
        if(resp != RdKafka::ERR_NO_ERROR) {
            CHAT_LOG_ERROR(g_logger) << "Failed to start consumer: " << RdKafka::err2str(resp) << std::endl;
            sleep(2);
        } else {
            break;
        }
    } while(m_running);

    if(m_ccb) {
        while(m_running) {
            m_consumer->consume_callback(t, m_partition, 1000, &cb, this);
            //poll(0);
        } 
    } else {
        while(m_running) {
            RdKafka::Message* msg = m_consumer->consume(t, partition, 1000);
            if(msg->topic()) {
                ++m_totalMsg;
                cb.consume_cb(*msg, this);
            }
            delete msg;
        }
    }

    m_consumer->stop(t, partition);
    m_consumer->poll(1000);
}

void KafkaConsumer::stop() {
    m_running = false;
}

// 接收到消息后更新偏移量。偏移量是指消费者在特定分区上消费消息的位置信息
void KafkaConsumer::updateOffset(const RdKafka::Message& msg) {
    {
        chat::RWMutex::ReadLock lock(m_mutex);
        auto it = m_partitionOffsets.find(msg.partition());
        if(it != m_partitionOffsets.end()) {
            chat::Atomic::compareAndSwap(it->second, it->second, msg.offset());
        } else {
            it = m_partitionTotal.find(msg.partition());
            if(it != m_partitionTotal.end()) {
                chat::Atomic::addFetch(it->second, 1);
            }
        }
    }

    chat::RWMutex::WriteLock lock(m_mutex);
    chat::Atomic::compareAndSwap(m_partitionOffsets[msg.partition()]
                              ,m_partitionOffsets[msg.partition()]
                              ,msg.offset());
    chat::Atomic::addFetch(m_partitionTotal[msg.partition()], 1);
}

void KafkaConsumer::listOffset(std::map<uint32_t, int64_t>& out) {
    chat::RWMutex::ReadLock lock(m_mutex);
    for(auto& i : m_partitionOffsets) {
        if(i.second > out[i.first]) {
            out[i.first] = i.second;
        }
    }
}

void KafkaConsumer::dump(std::ostream& os) {
    os << "KafkaConsumer broker_list=" << m_brokerList << std::endl;
    chat::RWMutex::ReadLock lock(m_mutex);
    os << "    offsets:" << std::endl;
    for(auto& i : m_partitionOffsets) {
        os << "        partition=" << i.first << " : " << i.second << std::endl;
    }
    uint64_t total = 0;
    os << "    totals: " << std::endl;
    for(auto& i : m_partitionTotal) {
        os << "        partition=" << i.first << " : " << i.second << std::endl;
        total += i.second;
    }
    os << "    total=" << total << std::endl;
    os << "    start_offset: " << std::endl;
    for(auto& i : m_partitionStartOffset) {
        os << "        partition=" << i.first << " : " << i.second << std::endl;
    }
}

KafkaConsumerGroup::KafkaConsumerGroup()
    :m_partitionStart(0)
    ,m_partitionEnd(0)
    ,m_partitionShare(10)
    ,m_defaultOffset(RD_KAFKA_OFFSET_END)
    ,m_snapInterval(60) {
    static chat::ConfigVar<std::string>::ptr g_system_path =
        chat::Config::Lookup<std::string>("system.path");
    m_snapFilePath = g_system_path->getValue();
}

KafkaConsumerGroup::~KafkaConsumerGroup() {
    if(m_timer) {
        m_timer->cancel();
    }
}

void KafkaConsumerGroup::start() {
    if(m_offsets.empty()) {
        auto file = m_snapFilePath + "/" + m_topic + ".tpc";
        std::ifstream ifs;
        if(chat::FSUtil::OpenForRead(ifs, file)) {
            std::string line;
            while(std::getline(ifs, line)) {
                if(line.empty()) {
                    continue;
                }
                size_t pos = line.find("=");
                if(pos == std::string::npos) {
                    continue;
                }
                m_offsets[chat::TypeUtil::Atoi(line.substr(0, pos))]
                        = chat::TypeUtil::Atoi(line.substr(pos + 1));
            }
        }
    }
    // 每个消费者消费m_partitionShare个分区
    int c = ceil((m_partitionEnd - m_partitionStart) * 1.0 / m_partitionShare);
    m_datas.resize(c);
    CHAT_LOG_INFO(g_logger) << "m_thread_name=" << m_threadName
               << " partition_start = " << m_partitionStart
               << " partition_end = " << m_partitionEnd;
    for(uint32_t i = m_partitionStart;
            i < m_partitionEnd; ++i) {
        int idx = (i - m_partitionStart) / m_partitionShare;
        auto kc = m_datas[idx];
        if(!kc) {
            kc.reset(new chat::KafkaConsumer);
            kc->setCcb(false);
            kc->setBrokerList(m_brokerList);
            kc->setCallback(m_cb);
            m_datas[idx] = kc;
        }
        chat::DynaThreadMgr::GetInstance()->dispatch(m_threadName, [this, i, kc](){
                CHAT_LOG_INFO(g_logger) << "start consumer topic=" << m_topic
                           << " partition=" << i;
                auto it = m_offsets.find(i);
                int64_t offset = m_defaultOffset;
                if(it != m_offsets.end() && it->second) {
                    offset = it->second;
                }
                kc->start(m_topic, i, offset, ""); 
        });
    }

    m_timer = chat::IOManager::GetThis()->addTimer(m_snapInterval * 1000, 
            [this](){
        auto file = m_snapFilePath + "/" + m_topic + ".tpc";
        std::ofstream ofs;
        if(chat::FSUtil::OpenForWrite(ofs, file)) {
            std::map<uint32_t, int64_t> offsets;
            listOffsets(offsets);
            for(auto& i : offsets) {
                ofs << i.first << "=" << i.second << std::endl;
            }
        } else {
            CHAT_LOG_ERROR(g_logger) << "open file error: " << file;
        }
    }, true);
}

void KafkaConsumerGroup::listOffsets(std::map<uint32_t, int64_t>& vs) {
    chat::RWMutex::ReadLock lock(m_mutex);
    for(auto& i : m_datas) {
        if(i) {
            i->listOffset(vs);
        }
    }
}

uint64_t KafkaConsumerGroup::getTotalMsg() {
    uint64_t total = 0;
    chat::RWMutex::ReadLock lock(m_mutex);
    for(auto& i : m_datas) {
        if(i) {
            total += i->getTotalMsg();
        }
    }
    return total;
}

std::ostream& KafkaConsumerGroup::dump(std::ostream& os) {
    os << "[KafkaConsumerGroup topic=" << m_topic
       << " partition=[" << m_partitionStart << ", "
       << m_partitionEnd << ") total=" << getTotalMsg()
       << " partition_share=" << m_partitionShare
       << " broker_list=["
       << m_brokerList << "]]";
    chat::RWMutex::ReadLock lock(m_mutex);
    for(auto& i : m_datas) {
        if(i) {
            os << "***********************************************************" << std::endl;
            i->dump(os);
        }
    }
    return os;
}

}