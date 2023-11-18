#include <iostream>
#include "../chat/log.h"
#include "../chat/util.h"

int main(int argc, char** argv) {
    chat::Logger::ptr logger(new chat::Logger);
    logger->addAppender(chat::LogAppender::ptr(new chat::StdoutLogAppender));

    chat::FileLogAppender::ptr file_appender(new chat::FileLogAppender("./log.txt"));
    chat::LogFormatter::ptr fmt(new chat::LogFormatter("%d%T%p%T%m%n"));
    file_appender->setFormatter(fmt);
    file_appender->setLevel(chat::LogLevel::ERROR);

    logger->addAppender(file_appender);

    CHAT_LOG_INFO(logger) << "test macro";
    CHAT_LOG_ERROR(logger) << "test macro error";

    CHAT_LOG_FMT_ERROR(logger, "test macro fmt error %s", "aa");

    auto l = chat::LoggerMgr::GetInstance()->getLogger("xx");
    CHAT_LOG_INFO(l) << "xxx";

    return 0;
}