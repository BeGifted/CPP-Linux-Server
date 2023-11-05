#ifndef __CHAT_UTIL_H__
#define __CHAT_UTIL_H__

#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <stdint.h>

namespace chat {
    
pid_t GetThreadId();
u_int32_t GetFiberId();

}


#endif