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
#include <boost/lexical_cast.hpp>

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


std::string base64decode(const std::string &src);
std::string base64encode(const std::string &data);
std::string base64encode(const void *data, size_t len);

// Returns result in hex
std::string md5(const std::string &data);
std::string sha1(const std::string &data);
// Returns result in blob
std::string md5sum(const std::string &data);
std::string md5sum(const void *data, size_t len);
std::string sha0sum(const std::string &data);
std::string sha0sum(const void *data, size_t len);
std::string sha1sum(const std::string &data);
std::string sha1sum(const void *data, size_t len);

/// Output must be of size len * 2, and will *not* be null-terminated
void hexstring_from_data(const void *data, size_t len, char *output);
std::string hexstring_from_data(const void *data, size_t len);
std::string hexstring_from_data(const std::string &data);

std::string random_string(size_t len, const std::string& chars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");

template<class V, class Map, class K>
V GetParamValue(const Map& m, const K& k, const V& def = V()) {
    auto it = m.find(k);
    if(it == m.end()) {
        return def;
    }
    try {
        return boost::lexical_cast<V>(it->second);
    } catch (...) {
    }
    return def;
}

}


#endif