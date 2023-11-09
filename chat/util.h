#ifndef __CHAT_UTIL_H__
#define __CHAT_UTIL_H__

#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace chat {
    
pid_t GetThreadId();
u_int32_t GetFiberId();

void Backtrace(std::vector<std::string>& bt, int size = 64, int skip=1);
std::string BacktraceToString(int size = 64, int skip=2, const std::string& prefix = "");
}


#endif