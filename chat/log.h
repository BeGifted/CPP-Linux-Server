#ifndef __CHAT_LOG_H__
#define __CHAT_LOG_H__

#include <string>
#include <stdint.h>
#include <memory>
#include <list>
#include <sstream>
#include <fstream>
#include <vector>
#include <iostream>
#include <map>
#include <functional>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include "util.h"
#include "singleton.h"
#include "thread.h"

/**
 * @brief 使用流式方式将日志级别level的日志写入到logger
 */
#define CHAT_LOG_LEVEL(logger, level) \
    if(logger->getLevel() <= level) \
        chat::LogEventWrap(std::make_shared<chat::LogEvent>(logger, level, \
                        __FILE__, __LINE__, 0, chat::GetThreadId(),\
                chat::GetFiberId(), time(0), chat::Thread::GetName())).getSS()

#define CHAT_LOG_DEBUG(logger) CHAT_LOG_LEVEL(logger, chat::LogLevel::DEBUG)
#define CHAT_LOG_INFO(logger) CHAT_LOG_LEVEL(logger, chat::LogLevel::INFO)
#define CHAT_LOG_WARN(logger) CHAT_LOG_LEVEL(logger, chat::LogLevel::WARN)
#define CHAT_LOG_ERROR(logger) CHAT_LOG_LEVEL(logger, chat::LogLevel::ERROR)
#define CHAT_LOG_FATAL(logger) CHAT_LOG_LEVEL(logger, chat::LogLevel::FATAL)

#define CHAT_LOG_FMT_LEVEL(logger, level, fmt, ...) \
    if(logger->getLevel() <= level) \
        chat::LogEventWrap(std::make_shared<chat::LogEvent>(logger, level, \
                        __FILE__, __LINE__, 0, chat::GetThreadId(),\
                chat::GetFiberId(), time(0), chat::Thread::GetName())).getEvent()->format(fmt, __VA_ARGS__)

#define CHAT_LOG_FMT_DEBUG(logger, fmt, ...) CHAT_LOG_FMT_LEVEL(logger, chat::LogLevel::DEBUG, fmt, __VA_ARGS__)
#define CHAT_LOG_FMT_INFO(logger, fmt, ...)  CHAT_LOG_FMT_LEVEL(logger, chat::LogLevel::INFO, fmt, __VA_ARGS__)
#define CHAT_LOG_FMT_WARN(logger, fmt, ...)  CHAT_LOG_FMT_LEVEL(logger, chat::LogLevel::WARN, fmt, __VA_ARGS__)
#define CHAT_LOG_FMT_ERROR(logger, fmt, ...) CHAT_LOG_FMT_LEVEL(logger, chat::LogLevel::ERROR, fmt, __VA_ARGS__)
#define CHAT_LOG_FMT_FATAL(logger, fmt, ...) CHAT_LOG_FMT_LEVEL(logger, chat::LogLevel::FATAL, fmt, __VA_ARGS__)

#define CHAT_LOG_ROOT() chat::LoggerMgr::GetInstance()->getRoot()
#define CHAT_LOG_NAME(name) chat::LoggerMgr::GetInstance()->getLogger(name)


namespace chat {
class Logger;
class LoggerManager;


//日志级别
class LogLevel {
public:
    enum Level {
        UNKNOWN = 0,
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        FATAL = 5
    };

    static const char* ToString(LogLevel::Level level);
    static LogLevel::Level FromString(const std::string& str);
};


//日志事件
class LogEvent {
public:
    typedef std::shared_ptr<LogEvent> ptr;
    LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level, const char* file, 
    int32_t line, uint32_t elapse, uint32_t thread_id, 
    uint32_t fiber_id, uint64_t time, const std::string& thread_name);

    const char* getFile() const { return m_file; }
    int32_t getLine() const { return m_line; }
    uint32_t getElapse() const { return m_elapse; }
    uint32_t getThreadId() const { return m_threadId; }
    uint32_t getFiberId() const { return m_fiberId; }
    uint32_t getTime() const { return m_time; }
    const std::string& getThreadName() const { return m_threadName; }
    std::string getContent() const { return m_ss.str(); }
    std::shared_ptr<Logger> getLogger() const { return m_logger;}
    LogLevel::Level getLevel() const { return m_level;}
    std::stringstream& getSS() { return m_ss; }

    void format(const char* fmt, ...);
    void format(const char* fmt, va_list al);
private:
    const char* m_file = nullptr;
    int32_t m_line = 0;
    uint32_t m_elapse = 0;  //程序启动到现在的毫秒数
    uint32_t m_threadId = 0;
    uint32_t m_fiberId = 0;
    uint64_t m_time = 0;
    std::string m_threadName;
    std::stringstream m_ss;
    std::shared_ptr<Logger> m_logger;
    LogLevel::Level m_level;

};

//日志事件包装器
class LogEventWrap {
public:
    LogEventWrap(LogEvent::ptr e);
    ~LogEventWrap();
    LogEvent::ptr getEvent() const { return m_event;}

    std::stringstream& getSS();
private:
    LogEvent::ptr m_event;
};


//日志格式器
class LogFormatter {
public:
    typedef std::shared_ptr<LogFormatter> ptr;
    LogFormatter(const std::string& pattern);

    std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);
    std::ostream& format(std::ostream& ofs, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);
public:
    class FormatItem {
    public:
        typedef std::shared_ptr<FormatItem> ptr;
        virtual ~FormatItem() {}
        virtual void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;
    };

    void init();

    bool isError() const { return m_error; }
    const std::string getPattern() const { return m_pattern;}
private:
    std::string m_pattern;
    std::vector<FormatItem::ptr> m_items;
    bool m_error = false;
};

//日志输出地
class LogAppender {
friend class Logger;
public:
    typedef std::shared_ptr<LogAppender> ptr;
    typedef Spinlock MutexType;
    virtual ~LogAppender() {}

    virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;
    virtual std::string toYamlString() = 0;
    void setFormatter(LogFormatter::ptr val);
    LogFormatter::ptr getFormatter();

    LogLevel::Level getLevel() const { return m_level;}
    void setLevel(LogLevel::Level val) { m_level = val;}
    virtual bool reopen() { return true;}
protected:
    LogLevel::Level m_level = LogLevel::DEBUG;
    bool m_hasFormatter = false;
    MutexType m_mutex;
    LogFormatter::ptr m_formatter;
};

//日志器
class Logger: public std::enable_shared_from_this<Logger> {
friend class LoggerManager;
public:
    typedef std::shared_ptr<Logger> ptr;
    typedef RWSpinlock MutexType;

    Logger(const std::string& name = "root");
    void log(LogLevel::Level level, LogEvent::ptr event);

    void debug(LogEvent::ptr event);
    void info(LogEvent::ptr event);
    void warn(LogEvent::ptr event);
    void error(LogEvent::ptr event);
    void fatal(LogEvent::ptr event);

    void addAppender(LogAppender::ptr appender);
    void delAppender(LogAppender::ptr appender);
    void clearAppenders();

    LogLevel::Level getLevel() const { return m_level; }
    void setLevel(LogLevel::Level val) { m_level = val; }

    void setFormatter(LogFormatter::ptr val);
    void setFormatter(const std::string& val);
    LogFormatter::ptr getFormatter();

    const std::string& getName() const { return m_name; }

    std::string toYamlString();

    bool reopen();

private:
    std::string m_name;
    LogLevel::Level m_level;
    MutexType m_mutex;
    std::list<LogAppender::ptr> m_appenders;
    LogFormatter::ptr m_formatter;
    Logger::ptr m_root;
};

//输出到控制台的Appender
class StdoutLogAppender: public LogAppender {
public:
    typedef std::shared_ptr<StdoutLogAppender> ptr;
    void log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override;
    std::string toYamlString() override;
private:
};

//输出到文件的Appender
class FileLogAppender: public LogAppender {
public:
    typedef std::shared_ptr<FileLogAppender> ptr;
    FileLogAppender(const std::string& filename);
    void log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override;
    std::string toYamlString() override;
    //重新打开文件
    bool reopen() override;
private:
    std::string m_filename;
    std::ofstream m_filestream;
    uint64_t m_lastTime = 0;
};

class LoggerManager {
public:
    typedef RWSpinlock MutexType;
    LoggerManager();
    Logger::ptr getLogger(const std::string& name);
    void init();

    Logger::ptr getRoot() const { return m_root; }
    std::string toYamlString();
    bool reopen();
private:
    MutexType m_mutex;
    std::map<std::string, Logger::ptr> m_loggers;
    Logger::ptr m_root;
};

typedef chat::Singleton<LoggerManager> LoggerMgr;

}

#endif