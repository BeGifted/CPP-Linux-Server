#include "util.h"
#include <execinfo.h>
#include "log.h"
#include "fiber.h"
#include <sys/time.h>

namespace chat {

static chat::Logger::ptr g_logger = CHAT_LOG_NAME("system");
    
pid_t GetThreadId() {
    return syscall(SYS_gettid);
}

uint32_t GetFiberId() {
    return chat::Fiber::GetFiberId();
}

void Backtrace(std::vector<std::string>& bt, int size, int skip) {
    void** array = (void**)malloc(sizeof(void*) * size);  //数组
    size_t s = ::backtrace(array, size);  //获取程序调用堆栈信息 在全局命名空间中调用 以确保不会与任何可能的局部函数或变量冲突

    char** strings = backtrace_symbols(array, s);
    if (strings == NULL) {
        CHAT_LOG_ERROR(g_logger) << "tracktrace_symbols error";
        return;
    }

    for (size_t i = skip; i < s; i++) {
        bt.push_back(strings[i]);
    }
    free(strings);
    free(array);
}

std::string BacktraceToString(int size, int skip, const std::string& prefix) {
    std::vector<std::string> bt;
    Backtrace(bt, size);
    std::stringstream ss;
    for (size_t i = 0; i < bt.size(); i++) {
        ss << prefix << bt[i] << std::endl;
    }
    return ss.str();
}

uint64_t GetCurrentMs() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000ul  + tv.tv_usec / 1000;
}

uint64_t GetCurrentUs() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 * 1000ul  + tv.tv_usec;
}

std::string Time2Str(time_t ts, const std::string& format) {
    struct tm tm;
    localtime_r(&ts, &tm);
    char buf[64];
    strftime(buf, sizeof(buf), format.c_str(), &tm);
    return buf;
}

time_t Str2Time(const char* str, const char* format) {
    struct tm t;
    memset(&t, 0, sizeof(t));
    if (!strptime(str, format, &t)) {
        return 0;
    }
    return mktime(&t);
}

}