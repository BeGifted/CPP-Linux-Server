#ifndef __CHAT_UTIL_H__
#define __CHAT_UTIL_H__

#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <string>
#include <iomanip>
#include <yaml-cpp/yaml.h>
#include <iostream>

namespace chat {
    
pid_t GetThreadId();
u_int32_t GetFiberId();

void Backtrace(std::vector<std::string>& bt, int size = 64, int skip=1);
std::string BacktraceToString(int size = 64, int skip=2, const std::string& prefix = "");

uint64_t GetCurrentMs();
uint64_t GetCurrentUs();

std::string Time2Str(time_t ts = time(0), const std::string& format = "%Y-%m-%d %H:%M:%S");
time_t Str2Time(const char* str, const char* format = "%Y-%m-%d %H:%M:%S");

class FSUtil {
public:
    static void ListAllFile(std::vector<std::string>& files
                            ,const std::string& path
                            ,const std::string& subfix);
    static bool Mkdir(const std::string& dirname);
    static bool IsRunningPidfile(const std::string& pidfile);
    static bool Rm(const std::string& path);
    static bool Mv(const std::string& from, const std::string& to);
    static bool Realpath(const std::string& path, std::string& rpath);
    static bool Symlink(const std::string& frm, const std::string& to);
    static bool Unlink(const std::string& filename, bool exist = false);
    static std::string Dirname(const std::string& filename);
    static std::string Basename(const std::string& filename);
    static bool OpenForRead(std::ifstream& ifs, const std::string& filename
                    ,std::ios_base::openmode mode);
    static bool OpenForWrite(std::ofstream& ofs, const std::string& filename
                    ,std::ios_base::openmode mode);
};


}


#endif