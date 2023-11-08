#include "util.h"
#include <execinfo.h>
#include "log.h"
#include "fiber.h"

namespace chat {

chat::Logger::ptr g_logger = CHAT_LOG_NAME("system");
    
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


}