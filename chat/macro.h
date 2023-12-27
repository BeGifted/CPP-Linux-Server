#ifndef __CHAT_MACRO_H__
#define __CHAT_MACRO_H__

//常用宏的封装 预编译时展开

#include <string.h>
#include <assert.h>
#include "log.h"
#include "util.h"

#if defined __GNUC__ || defined __llvm__
// LIKELY 宏的封装, 告诉编译器优化 ,条件大概率成立
#   define CHAT_LIKELY(x)       __builtin_expect(!!(x), 1)
// LIKELY 宏的封装, 告诉编译器优化, 条件大概率不成立
#   define CHAT_UNLIKELY(x)     __builtin_expect(!!(x), 0)
#else
#   define CHAT_LIKELY(x)      (x)
#   define CHAT_UNLIKELY(x)      (x)
#endif

// 断言宏封装
#define CHAT_ASSERT(x) \
    if(CHAT_UNLIKELY(!(x))) { \
        CHAT_LOG_ERROR(CHAT_LOG_ROOT()) << "ASSERTION: " #x \
            << "\nbacktrace:\n" \
            << chat::BacktraceToString(100, 2, "    "); \
        assert(x); \
    }

// 断言宏封装
#define CHAT_ASSERT2(x, w) \
    if(CHAT_UNLIKELY(!(x))) { \
        CHAT_LOG_ERROR(CHAT_LOG_ROOT()) << "ASSERTION: " #x \
            << "\n" << w \
            << "\nbacktrace:\n" \
            << chat::BacktraceToString(100, 2, "    "); \
        assert(x); \
    }

#define CHAT_STR(...) #__VA_ARGS__
#define CHAT_TSTR(...) _CHAT_STR(__VA_ARGS__)
#define CHAT_DSTR(s, d) (NULL != (s) ? (const char*)(s) : (const char*)(d))
#define CHAT_SSTR(s) CHAT_DSTR(s, "")
#define CHAT_SLEN(s) (NULL != (s) ? strlen((const char*)(s)) : 0)

#endif