#ifndef __CHAT_UTIL_H__
#define __CHAT_UTIL_H__

#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <iomanip>
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <boost/lexical_cast.hpp>
#include <jsoncpp/json/json.h>
#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <memory>
#include <google/protobuf/message.h>
#include <cxxabi.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <unordered_map>
#include "chat/iomanager.h"
#include "chat/singleton.h"

namespace chat {
    
pid_t GetThreadId();
u_int32_t GetFiberId();

void Backtrace(std::vector<std::string>& bt, int size = 64, int skip=1);
std::string BacktraceToString(int size = 64, int skip=2, const std::string& prefix = "");

uint64_t GetCurrentMs();
uint64_t GetCurrentUs();

std::string Time2Str(time_t ts = time(0), const std::string& format = "%Y-%m-%d %H:%M:%S");
time_t Str2Time(const char* str, const char* format = "%Y-%m-%d %H:%M:%S");

std::string ToUpper(const std::string& name);
std::string ToLower(const std::string& name);

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
                    ,std::ios_base::openmode mode = std::ios_base::in);
    static bool OpenForWrite(std::ofstream& ofs, const std::string& filename
                    ,std::ios_base::openmode mode = std::ios_base::out);
};


std::string base64decode(const std::string &src, bool url = false);
std::string base64encode(const std::string &data, bool url = false);
std::string base64encode(const void *data, size_t len, bool url = false);

// Returns result in hex
std::string md5(const std::string &data);
std::string sha1(const std::string &data);
// Returns result in blob
std::string md5sum(const std::string &data);
std::string md5sum(const void *data, size_t len);
// std::string sha0sum(const std::string &data);
// std::string sha0sum(const void *data, size_t len);
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

template<class T>
void delete_array(T* v) {
    if(v) {
        delete[] v;
    }
}

std::string GetIPv4();

class TypeUtil {
public:
    static int8_t ToChar(const std::string& str);
    static int64_t Atoi(const std::string& str);
    static double Atof(const std::string& str);
    static int8_t ToChar(const char* str);
    static int64_t Atoi(const char* str);
    static double Atof(const char* str);
};

class JsonUtil {
public:
    static bool NeedEscape(const std::string& v);
    static std::string Escape(const std::string& v);

    static std::string GetString(const Json::Value& json
                          ,const std::string& name
                          ,const std::string& default_value = "");
    static double GetDouble(const Json::Value& json
                     ,const std::string& name
                     ,double default_value = 0);
    static int32_t GetInt32(const Json::Value& json
                     ,const std::string& name
                     ,int32_t default_value = 0);
    static uint32_t GetUint32(const Json::Value& json
                       ,const std::string& name
                       ,uint32_t default_value = 0);
    static int64_t GetInt64(const Json::Value& json
                     ,const std::string& name
                     ,int64_t default_value = 0);
    static uint64_t GetUint64(const Json::Value& json
                       ,const std::string& name
                       ,uint64_t default_value = 0);

    static std::string GetStringValue(const Json::Value& json
                          ,const std::string& default_value = "");
    static double GetDoubleValue(const Json::Value& json
                     ,double default_value = 0);
    static int32_t GetInt32Value(const Json::Value& json
                     ,int32_t default_value = 0);
    static uint32_t GetUint32Value(const Json::Value& json
                       ,uint32_t default_value = 0);
    static int64_t GetInt64Value(const Json::Value& json
                     ,int64_t default_value = 0);
    static uint64_t GetUint64Value(const Json::Value& json
                       ,uint64_t default_value = 0);


    static bool FromString(Json::Value& json, const std::string& v);
    static std::string ToString(const Json::Value& json, bool emit_utf8 = true);
};

class Atomic {
public:
    template<class T, class S>
    static T addFetch(volatile T& t, S v = 1) {
        return __sync_add_and_fetch(&t, (T)v);
    }
    template<class T, class S>
    static T subFetch(volatile T& t, S v = 1) {
        return __sync_sub_and_fetch(&t, (T)v);
    }
    template<class T, class S>
    static T orFetch(volatile T& t, S v) {
        return __sync_or_and_fetch(&t, (T)v);
    }
    template<class T, class S>
    static T andFetch(volatile T& t, S v) {
        return __sync_and_and_fetch(&t, (T)v);
    }
    template<class T, class S>
    static T xorFetch(volatile T& t, S v) {
        return __sync_xor_and_fetch(&t, (T)v);
    }
    template<class T, class S>
    static T nandFetch(volatile T& t, S v) {
        return __sync_nand_and_fetch(&t, (T)v);
    }
    template<class T, class S>
    static T fetchAdd(volatile T& t, S v = 1) {
        return __sync_fetch_and_add(&t, (T)v);
    }
    template<class T, class S>
    static T fetchSub(volatile T& t, S v = 1) {
        return __sync_fetch_and_sub(&t, (T)v);
    }
    template<class T, class S>
    static T fetchOr(volatile T& t, S v) {
        return __sync_fetch_and_or(&t, (T)v);
    }
    template<class T, class S>
    static T fetchAnd(volatile T& t, S v) {
        return __sync_fetch_and_and(&t, (T)v);
    }
    template<class T, class S>
    static T fetchXor(volatile T& t, S v) {
        return __sync_fetch_and_xor(&t, (T)v);
    }
    template<class T, class S>
    static T fetchNand(volatile T& t, S v) {
        return __sync_fetch_and_nand(&t, (T)v);
    }
    template<class T, class S>
    static T compareAndSwap(volatile T& t, S old_val, S new_val) {
        return __sync_val_compare_and_swap(&t, (T)old_val, (T)new_val);
    }

    template<class T, class S>
    static bool compareAndSwapBool(volatile T& t, S old_val, S new_val) {
        return __sync_bool_compare_and_swap(&t, (T)old_val, (T)new_val);
    }
};

class CryptoUtil {
public:
    //key 32字节
    static int32_t AES256Ecb(const void* key
                            ,const void* in
                            ,int32_t in_len
                            ,void* out
                            ,bool encode);

    //key 16字节
    static int32_t AES128Ecb(const void* key
                            ,const void* in
                            ,int32_t in_len
                            ,void* out
                            ,bool encode);

    //key 32字节
    //iv 16字节
    static int32_t AES256Cbc(const void* key, const void* iv
                            ,const void* in, int32_t in_len
                            ,void* out, bool encode);

    //key 16字节
    //iv 16字节
    static int32_t AES128Cbc(const void* key, const void* iv
                            ,const void* in, int32_t in_len
                            ,void* out, bool encode);

    static int32_t Crypto(const EVP_CIPHER* cipher, bool enc
                          ,const void* key, const void* iv
                          ,const void* in, int32_t in_len
                          ,void* out, int32_t* out_len);
};


class RSACipher {
public:
    typedef std::shared_ptr<RSACipher> ptr;

    static int32_t GenerateKey(const std::string& pubkey_file
                               ,const std::string& prikey_file
                               ,uint32_t length = 1024);

    static RSACipher::ptr Create(const std::string& pubkey_file
                                ,const std::string& prikey_file);

    RSACipher();
    ~RSACipher();

    int32_t privateEncrypt(const void* from, int flen,
                           void* to, int padding = RSA_NO_PADDING);
    int32_t publicEncrypt(const void* from, int flen,
                           void* to, int padding = RSA_NO_PADDING);
    int32_t privateDecrypt(const void* from, int flen,
                           void* to, int padding = RSA_NO_PADDING);
    int32_t publicDecrypt(const void* from, int flen,
                           void* to, int padding = RSA_NO_PADDING);
    int32_t privateEncrypt(const void* from, int flen,
                           std::string& to, int padding = RSA_NO_PADDING);
    int32_t publicEncrypt(const void* from, int flen,
                           std::string& to, int padding = RSA_NO_PADDING);
    int32_t privateDecrypt(const void* from, int flen,
                           std::string& to, int padding = RSA_NO_PADDING);
    int32_t publicDecrypt(const void* from, int flen,
                           std::string& to, int padding = RSA_NO_PADDING);


    const std::string& getPubkeyStr() const { return m_pubkeyStr;}
    const std::string& getPrikeyStr() const { return m_prikeyStr;}

    int32_t getPubRSASize();
    int32_t getPriRSASize();
private:
    RSA* m_pubkey;
    RSA* m_prikey;
    std::string m_pubkeyStr;
    std::string m_prikeyStr;
};

class StringUtil {
public:
    static std::string Format(const char* fmt, ...);
    static std::string Formatv(const char* fmt, va_list ap);

    static std::string UrlEncode(const std::string& str, bool space_as_plus = true);
    static std::string UrlDecode(const std::string& str, bool space_as_plus = true);

    static std::string Trim(const std::string& str, const std::string& delimit = " \t\r\n");
    static std::string TrimLeft(const std::string& str, const std::string& delimit = " \t\r\n");
    static std::string TrimRight(const std::string& str, const std::string& delimit = " \t\r\n");

    static std::string WStringToString(const std::wstring& ws);
    static std::wstring StringToWString(const std::string& s);
};

template<class T>
const char* TypeToName() {
    static const char* s_name = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, nullptr);
    return s_name;
}

std::string PBToJsonString(const google::protobuf::Message& message);

template<class Iter>
std::string Join(Iter begin, Iter end, const std::string& tag) {
    std::stringstream ss;
    for(Iter it = begin; it != end; ++it) {
        if(it != begin) {
            ss << tag;
        }
        ss << *it;
    }
    return ss.str();
}

// std::vector<std::string> split(const std::string& str, char delimiter) {
//     std::vector<std::string> tokens;
//     std::string token;
//     std::istringstream tokenStream(str);
    
//     while (std::getline(tokenStream, token, delimiter)) {
//         tokens.push_back(token);
//     }
    
//     return tokens;
// }


std::string GetHostName();
std::string GetIPv4();


std::vector<std::string> split(const std::string &str, char delim, size_t max = ~0);
std::vector<std::string> split(const std::string &str, const char *delims, size_t max = ~0);

//时间微秒
class TimeCalc {
public:
    TimeCalc();
    uint64_t elapse() const;

    void tick(const std::string& name);
    const std::vector<std::pair<std::string, uint64_t> > getTimeLine() const { return m_timeLine;}

    std::string toString() const;
private:
    uint64_t m_time;
    std::vector<std::pair<std::string, uint64_t> > m_timeLine;
};

template<class T, class ...Args>
inline std::shared_ptr<T> protected_make_shared(Args&&... args) {
    struct Helper : T {
        Helper(Args&&... args)
            :T(std::forward<Args>(args)...) {
        }
    };
    return std::make_shared<Helper>(std::forward<Args>(args)...);
}

class TrackerManager;
class Tracker : public std::enable_shared_from_this<Tracker> {
friend class TrackerManager;
public:
    typedef std::shared_ptr<Tracker> ptr;
    typedef std::function<void(const std::vector<std::string>& dims
            ,int64_t* datas, int64_t interval)> callback;

    Tracker();
    ~Tracker();

    void start();
    void stop();

    int64_t inc(uint32_t key, int64_t v);
    int64_t dec(uint32_t key, int64_t v);

    void addDim(uint32_t key, const std::string& name);

    int64_t  getInterval() const { return m_interval;}
    void setInterval(int64_t v) { m_interval = v;}

    const std::string& getName() const { return m_name;}
    void setName(const std::string& v) { m_name = v;}

    void toData(std::map<std::string, int64_t>& data, const std::set<uint32_t>& times = {});
    void toDurationData(std::map<std::string, int64_t>& data, uint32_t duration = 1, bool with_time = true);
    void toTimeData(std::map<std::string, int64_t>& data, uint32_t ts = 1, bool with_time = true);
private:
    void onTimer();

    int64_t privateInc(uint32_t key, int64_t v);
    int64_t privateDec(uint32_t key, int64_t v);
    void init();
private:
    std::string m_name;
    std::vector<std::string> m_dims;
    int64_t* m_datas;
    uint32_t m_interval;
    uint32_t m_dimCount;
    chat::Timer::ptr m_timer;
};

class TrackerManager {
public:
    typedef chat::RWMutex RWMutexType;
    TrackerManager();
    void addDim(uint32_t key, const std::string& name);

    int64_t inc(const std::string& name, uint32_t key, int64_t v);
    int64_t dec(const std::string& name, uint32_t key, int64_t v);

    int64_t  getInterval() const { return m_interval;}
    void setInterval(int64_t v) { m_interval = v;}

    Tracker::ptr getOrCreate(const std::string& name);
    Tracker::ptr get(const std::string& name);

    std::string toString(uint32_t duration = 10);

    void start();
private:
    RWMutexType m_mutex;
    uint32_t m_interval;
    std::vector<std::string> m_dims;
    std::unordered_map<std::string, Tracker::ptr> m_datas;
    Tracker::ptr m_totalTracker;
    chat::Timer::ptr m_timer;
};

typedef chat::Singleton<TrackerManager> TrackerMgr;

template<class Iter>
std::string MapJoin(Iter begin, Iter end, const std::string& tag1 = "=", const std::string& tag2 = "&") {
    std::stringstream ss;
    for(Iter it = begin; it != end; ++it) {
        if(it != begin) {
            ss << tag2;
        }
        ss << it->first << "=" << it->second;
    }
    return ss.str();
}


}

#endif