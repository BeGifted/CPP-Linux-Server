#include "util.h"
#include <execinfo.h>
#include "log.h"
#include "fiber.h"
#include <sys/time.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <signal.h>

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


void FSUtil::ListAllFile(std::vector<std::string>& files
                            ,const std::string& path
                            ,const std::string& subfix) {
    if(access(path.c_str(), 0) != 0) {
        return;
    }
    DIR* dir = opendir(path.c_str());
    if(dir == nullptr) {
        return;
    }
    struct dirent* dp = nullptr;
    while((dp = readdir(dir)) != nullptr) {
        if(dp->d_type == DT_DIR) {
            if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) {
                continue;
            }
            ListAllFile(files, path + "/" + dp->d_name, subfix);
        } else if (dp->d_type == DT_REG) {
            std::string filename(dp->d_name);
            if (subfix.empty()) {
                files.push_back(path + "/" + filename);
            } else {
                if (filename.size() < subfix.size()) {
                    continue;
                }
                if (filename.substr(filename.length() - subfix.size()) == subfix) {
                    files.push_back(path + "/" + filename);
                }
            }
        }
    }
    closedir(dir);
}

static int __lstat(const char* file, struct stat* st = nullptr) {
    struct stat lst;
    int ret = lstat(file, &lst);
    if(st) {
        *st = lst;
    }
    return ret;
}

static int __mkdir(const char* dirname) {
    if(access(dirname, F_OK) == 0) {
        return 0;
    }
    return mkdir(dirname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

bool FSUtil::Mkdir(const std::string& dirname) {
    if(__lstat(dirname.c_str()) == 0) {
        return true;
    }
    char* path = strdup(dirname.c_str());
    char* ptr = strchr(path + 1, '/');
    do {
        for(; ptr; *ptr = '/', ptr = strchr(ptr + 1, '/')) {
            *ptr = '\0';
            if(__mkdir(path) != 0) {
                break;
            }
        }
        if(ptr != nullptr) {
            break;
        } else if(__mkdir(path) != 0) {
            break;
        }
        free(path);
        return true;
    } while(0);
    free(path);
    return false;
}

bool FSUtil::IsRunningPidfile(const std::string& pidfile) {
    if(__lstat(pidfile.c_str()) != 0) {
        return false;
    }
    std::ifstream ifs(pidfile);
    std::string line;
    if(!ifs || !std::getline(ifs, line)) {
        return false;
    }
    if(line.empty()) {
        return false;
    }
    pid_t pid = atoi(line.c_str());
    if(pid <= 1) {
        return false;
    }
    if(kill(pid, 0) != 0) {
        return false;
    }
    return true;
}

bool FSUtil::Unlink(const std::string& filename, bool exist) {
    if(!exist && __lstat(filename.c_str())) {
        return true;
    }
    return ::unlink(filename.c_str()) == 0;
}

bool FSUtil::Rm(const std::string& path) {
    struct stat st;
    if(lstat(path.c_str(), &st)) {
        return true;
    }
    if(!(st.st_mode & S_IFDIR)) {
        return Unlink(path);
    }

    DIR* dir = opendir(path.c_str());
    if(!dir) {
        return false;
    }
    
    bool ret = true;
    struct dirent* dp = nullptr;
    while((dp = readdir(dir))) {
        if(!strcmp(dp->d_name, ".")
                || !strcmp(dp->d_name, "..")) {
            continue;
        }
        std::string dirname = path + "/" + dp->d_name;
        ret = Rm(dirname);
    }
    closedir(dir);
    if(::rmdir(path.c_str())) {
        ret = false;
    }
    return ret;
}

bool FSUtil::Mv(const std::string& from, const std::string& to) {
    if(!Rm(to)) {
        return false;
    }
    return rename(from.c_str(), to.c_str()) == 0;
}

bool FSUtil::Realpath(const std::string& path, std::string& rpath) {
    if(__lstat(path.c_str())) {
        return false;
    }
    char* ptr = ::realpath(path.c_str(), nullptr);
    if(nullptr == ptr) {
        return false;
    }
    std::string(ptr).swap(rpath);
    free(ptr);
    return true;
}

bool FSUtil::Symlink(const std::string& from, const std::string& to) {
    if(!Rm(to)) {
        return false;
    }
    return ::symlink(from.c_str(), to.c_str()) == 0;
}

std::string FSUtil::Dirname(const std::string& filename) {
    if(filename.empty()) {
        return ".";
    }
    auto pos = filename.rfind('/');
    if(pos == 0) {
        return "/";
    } else if(pos == std::string::npos) {
        return ".";
    } else {
        return filename.substr(0, pos);
    }
}

std::string FSUtil::Basename(const std::string& filename) {
    if(filename.empty()) {
        return filename;
    }
    auto pos = filename.rfind('/');
    if(pos == std::string::npos) {
        return filename;
    } else {
        return filename.substr(pos + 1);
    }
}

bool FSUtil::OpenForRead(std::ifstream& ifs, const std::string& filename
                        ,std::ios_base::openmode mode) {
    ifs.open(filename.c_str(), mode);
    return ifs.is_open();
}

bool FSUtil::OpenForWrite(std::ofstream& ofs, const std::string& filename
                        ,std::ios_base::openmode mode) {
    ofs.open(filename.c_str(), mode);   
    if(!ofs.is_open()) {
        std::string dir = Dirname(filename);
        Mkdir(dir);
        ofs.open(filename.c_str(), mode);
    }
    return ofs.is_open();
}

std::string base64decode(const std::string &src) {
    std::string result;
    result.resize(src.size() * 3 / 4);
    char *writeBuf = &result[0];

    const char* ptr = src.c_str();
    const char* end = ptr + src.size();

    while(ptr < end) {
        int i = 0;
        int padding = 0;
        int packed = 0;
        for(; i < 4 && ptr < end; ++i, ++ptr) {
            if(*ptr == '=') {
                ++padding;
                packed <<= 6;
                continue;
            }

            // padding with "=" only
            if (padding > 0) {
                return "";
            }

            int val = 0;
            if(*ptr >= 'A' && *ptr <= 'Z') {
                val = *ptr - 'A';
            } else if(*ptr >= 'a' && *ptr <= 'z') {
                val = *ptr - 'a' + 26;
            } else if(*ptr >= '0' && *ptr <= '9') {
                val = *ptr - '0' + 52;
            } else if(*ptr == '+') {
                val = 62;
            } else if(*ptr == '/') {
                val = 63;
            } else {
                return ""; // invalid character
            }

            packed = (packed << 6) | val;
        }
        if (i != 4) {
            return "";
        }
        if (padding > 0 && ptr != end) {
            return "";
        }
        if (padding > 2) {
            return "";
        }

        *writeBuf++ = (char)((packed >> 16) & 0xff);
        if(padding != 2) {
            *writeBuf++ = (char)((packed >> 8) & 0xff);
        }
        if(padding == 0) {
            *writeBuf++ = (char)(packed & 0xff);
        }
    }

    result.resize(writeBuf - result.c_str());
    return result;
}

std::string base64encode(const std::string& data) {
    return base64encode(data.c_str(), data.size());
}

std::string base64encode(const void* data, size_t len) {
    const char* base64 =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string ret;
    ret.reserve(len * 4 / 3 + 2);

    const unsigned char* ptr = (const unsigned char*)data;
    const unsigned char* end = ptr + len;

    while(ptr < end) {
        unsigned int packed = 0;
        int i = 0;
        int padding = 0;
        for(; i < 3 && ptr < end; ++i, ++ptr) {
            packed = (packed << 8) | *ptr;
        }
        if(i == 2) {
            padding = 1;
        } else if (i == 1) {
            padding = 2;
        }
        for(; i < 3; ++i) {
            packed <<= 8;
        }

        ret.append(1, base64[packed >> 18]);
        ret.append(1, base64[(packed >> 12) & 0x3f]);
        if(padding != 2) {
            ret.append(1, base64[(packed >> 6) & 0x3f]);
        }
        if(padding == 0) {
            ret.append(1, base64[packed & 0x3f]);
        }
        ret.append(padding, '=');
    }

    return ret;
}

std::string md5(const std::string &data) {
    return hexstring_from_data(md5sum(data).c_str(), MD5_DIGEST_LENGTH);
}

std::string sha1(const std::string &data) {
    return hexstring_from_data(sha1sum(data).c_str(), SHA_DIGEST_LENGTH);
}

std::string md5sum(const void *data, size_t len) {
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, data, len);
    std::string result;
    result.resize(MD5_DIGEST_LENGTH);
    MD5_Final((unsigned char*)&result[0], &ctx);
    return result;
}

std::string md5sum(const std::string &data) {
    return md5sum(data.c_str(), data.size());
}

// std::string sha0sum(const void *data, size_t len) {
//     SHA_CTX ctx;
//     SHA_Init(&ctx);
//     SHA_Update(&ctx, data, len);
//     std::string result;
//     result.resize(SHA_DIGEST_LENGTH);
//     SHA_Final((unsigned char*)&result[0], &ctx);
//     return result;
// }

// std::string sha0sum(const std::string & data) {
//     return sha0sum(data.c_str(), data.length());
// }

std::string sha1sum(const void *data, size_t len) {
    SHA_CTX ctx;
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, data, len);
    std::string result;
    result.resize(SHA_DIGEST_LENGTH);
    SHA1_Final((unsigned char*)&result[0], &ctx);
    return result;
}

std::string sha1sum(const std::string &data) {
    return sha1sum(data.c_str(), data.size());
}

void hexstring_from_data(const void *data, size_t len, char *output) {
    const unsigned char *buf = (const unsigned char *)data;
    size_t i, j;
    for (i = j = 0; i < len; ++i) {
        char c;
        c = (buf[i] >> 4) & 0xf;
        c = (c > 9) ? c + 'a' - 10 : c + '0';
        output[j++] = c;
        c = (buf[i] & 0xf);
        c = (c > 9) ? c + 'a' - 10 : c + '0';
        output[j++] = c;
    }
}

std::string hexstring_from_data(const void *data, size_t len) {
    if (len == 0) {
        return std::string();
    }
    std::string result;
    result.resize(len * 2);
    hexstring_from_data(data, len, &result[0]);
    return result;
}

std::string hexstring_from_data(const std::string &data) {
    return hexstring_from_data(data.c_str(), data.size());
}

std::string random_string(size_t len, const std::string& chars) {
    if(len == 0 || chars.empty()) {
        return "";
    }
    std::string rt;
    rt.resize(len);
    int count = chars.size();
    for(size_t i = 0; i < len; ++i) {
        rt[i] = chars[rand() % count];
    }
    return rt;
}

in_addr_t GetIPv4Inet() {
    struct ifaddrs* ifas = nullptr;
    struct ifaddrs* ifa = nullptr;

    in_addr_t localhost = inet_addr("127.0.0.1");
    if(getifaddrs(&ifas)) {
        CHAT_LOG_ERROR(g_logger) << "getifaddrs errno=" << errno
            << " errstr=" << strerror(errno);
        return localhost;
    }

    in_addr_t ipv4 = localhost;

    for(ifa = ifas; ifa && ifa->ifa_addr; ifa = ifa->ifa_next) {
        if(ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        if(!strncasecmp(ifa->ifa_name, "lo", 2)) {
            continue;
        }
        ipv4 = ((struct sockaddr_in*)ifa->ifa_addr)->sin_addr.s_addr;
        if(ipv4 == localhost) {
            continue;
        }
    }
    if(ifas != nullptr) {
        freeifaddrs(ifas);
    }
    return ipv4;
}

std::string _GetIPv4() {
    std::shared_ptr<char> ipv4(new char[INET_ADDRSTRLEN], chat::delete_array<char>);
    memset(ipv4.get(), 0, INET_ADDRSTRLEN);
    auto ia = GetIPv4Inet();
    inet_ntop(AF_INET, &ia, ipv4.get(), INET_ADDRSTRLEN);
    return ipv4.get();
}

std::string GetIPv4() {
    static const std::string ip = _GetIPv4();
    return ip;
}


int8_t  TypeUtil::ToChar(const std::string& str) {
    if(str.empty()) {
        return 0;
    }
    return *str.begin();
}

int64_t TypeUtil::Atoi(const std::string& str) {
    if(str.empty()) {
        return 0;
    }
    return strtoull(str.c_str(), nullptr, 10);
}

double  TypeUtil::Atof(const std::string& str) {
    if(str.empty()) {
        return 0;
    }
    return atof(str.c_str());
}

int8_t  TypeUtil::ToChar(const char* str) {
    if(str == nullptr) {
        return 0;
    }
    return str[0];
}

int64_t TypeUtil::Atoi(const char* str) {
    if(str == nullptr) {
        return 0;
    }
    return strtoull(str, nullptr, 10);
}

double  TypeUtil::Atof(const char* str) {
    if(str == nullptr) {
        return 0;
    }
    return atof(str);
}



bool JsonUtil::NeedEscape(const std::string& v) {
    for(auto& c : v) {
        switch(c) {
            case '\f':
            case '\t':
            case '\r':
            case '\n':
            case '\b':
            case '"':
            case '\\':
                return true;
            default:
                break;
        }
    }
    return false;
}

std::string JsonUtil::Escape(const std::string& v) {
    size_t size = 0;
    for(auto& c : v) {
        switch(c) {
            case '\f':
            case '\t':
            case '\r':
            case '\n':
            case '\b':
            case '"':
            case '\\':
                size += 2;
                break;
            default:
                size += 1;
                break;
        }
    }
    if(size == v.size()) {
        return v;
    }

    std::string rt;
    rt.resize(size);
    for(auto& c : v) {
        switch(c) {
            case '\f':
                rt.append("\\f");
                break;
            case '\t':
                rt.append("\\t");
                break;
            case '\r':
                rt.append("\\r");
                break;
            case '\n':
                rt.append("\\n");
                break;
            case '\b':
                rt.append("\\b");
                break;
            case '"':
                rt.append("\\\"");
                break;
            case '\\':
                rt.append("\\\\");
                break;
            default:
                rt.append(1, c);
                break;

        }
    }
    return rt;
}

std::string JsonUtil::GetString(const Json::Value& json
                      ,const std::string& name
                      ,const std::string& default_value) {
    if(!json.isMember(name)) {
        return default_value;
    }
    auto& v = json[name];
    if(v.isString()) {
        return v.asString();
    }
    return default_value;
}

double JsonUtil::GetDouble(const Json::Value& json
                 ,const std::string& name
                 ,double default_value) {
    if(!json.isMember(name)) {
        return default_value;
    }
    auto& v = json[name];
    if(v.isDouble()) {
        return v.asDouble();
    } else if(v.isString()) {
        return TypeUtil::Atof(v.asString());
    }
    return default_value;
}

int32_t JsonUtil::GetInt32(const Json::Value& json
                 ,const std::string& name
                 ,int32_t default_value) {
    if(!json.isMember(name)) {
        return default_value;
    }
    auto& v = json[name];
    if(v.isInt()) {
        return v.asInt();
    } else if(v.isString()) {
        return TypeUtil::Atoi(v.asString());
    }
    return default_value;
}

uint32_t JsonUtil::GetUint32(const Json::Value& json
                   ,const std::string& name
                   ,uint32_t default_value) {
    if(!json.isMember(name)) {
        return default_value;
    }
    auto& v = json[name];
    if(v.isUInt()) {
        return v.asUInt();
    } else if(v.isString()) {
        return TypeUtil::Atoi(v.asString());
    }
    return default_value;
}

int64_t JsonUtil::GetInt64(const Json::Value& json
                 ,const std::string& name
                 ,int64_t default_value) {
    if(!json.isMember(name)) {
        return default_value;
    }
    auto& v = json[name];
    if(v.isInt64()) {
        return v.asInt64();
    } else if(v.isString()) {
        return TypeUtil::Atoi(v.asString());
    }
    return default_value;
}

uint64_t JsonUtil::GetUint64(const Json::Value& json
                   ,const std::string& name
                   ,uint64_t default_value) {
    if(!json.isMember(name)) {
        return default_value;
    }
    auto& v = json[name];
    if(v.isUInt64()) {
        return v.asUInt64();
    } else if(v.isString()) {
        return TypeUtil::Atoi(v.asString());
    }
    return default_value;
}

bool JsonUtil::FromString(Json::Value& json, const std::string& v) {
    Json::Reader reader;
    return reader.parse(v, json);
}

std::string JsonUtil::ToString(const Json::Value& json) {
    Json::FastWriter w;
    return w.write(json);
}

int32_t CryptoUtil::AES256Ecb(const void* key
                                ,const void* in
                                ,int32_t in_len
                                ,void* out
                                ,bool encode) {
    int32_t len = 0;
    return Crypto(EVP_aes_256_ecb(), encode, (const uint8_t*)key
                    ,nullptr, (const uint8_t*)in, in_len
                    ,(uint8_t*)out, &len);
}

int32_t CryptoUtil::AES128Ecb(const void* key
                                ,const void* in
                                ,int32_t in_len
                                ,void* out
                                ,bool encode) {
    int32_t len = 0;
    return Crypto(EVP_aes_128_ecb(), encode, (const uint8_t*)key
                    ,nullptr, (const uint8_t*)in, in_len
                    ,(uint8_t*)out, &len);

}

int32_t CryptoUtil::AES256Cbc(const void* key, const void* iv
                                ,const void* in, int32_t in_len
                                ,void* out, bool encode) {
    int32_t len = 0;
    return Crypto(EVP_aes_256_cbc(), encode, (const uint8_t*)key
                    ,(const uint8_t*)iv, (const uint8_t*)in, in_len
                    ,(uint8_t*)out, &len);

}

int32_t CryptoUtil::AES128Cbc(const void* key, const void* iv
                                ,const void* in, int32_t in_len
                                ,void* out, bool encode) {
    int32_t len = 0;
    return Crypto(EVP_aes_128_cbc(), encode, (const uint8_t*)key
                    ,(const uint8_t*)iv, (const uint8_t*)in, in_len
                    ,(uint8_t*)out, &len);
}

int32_t CryptoUtil::Crypto(const EVP_CIPHER* cipher, bool enc
                           ,const void* key, const void* iv
                           ,const void* in, int32_t in_len
                           ,void* out, int32_t* out_len) {
    int tmp_len = 0;
    bool has_error = false;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    do {
        //static CryptoInit s_crypto_init;
        EVP_CIPHER_CTX_init(ctx);
        EVP_CipherInit_ex(ctx, cipher, nullptr, (const uint8_t*)key
                ,(const uint8_t*)iv, enc);
        if(EVP_CipherUpdate(ctx, (uint8_t*)out, &tmp_len, (const uint8_t*)in, in_len) != 1) {
            has_error = true;
            break;
        }
        *out_len = tmp_len;
        if(EVP_CipherFinal_ex(ctx, (uint8_t*)out + tmp_len, &tmp_len) != 1) {
            has_error = true;
            break;
        }
        *out_len += tmp_len;
    } while(0);
    EVP_CIPHER_CTX_cleanup(ctx);
    if(has_error) {
        return -1;
    }
    return *out_len;
}

int32_t RSACipher::GenerateKey(const std::string& pubkey_file
                               ,const std::string& prikey_file
                               ,uint32_t length) {
    int rt = 0;
    FILE* fp = nullptr;
    RSA* rsa = nullptr;
    do {
        rsa = RSA_generate_key(length, RSA_F4, NULL, NULL);
        //rsa = RSA_generate_key(1024, RSA_F4, NULL, NULL);
        if(!rsa) {
            rt = -1;
            break;
        }

        fp = fopen(pubkey_file.c_str(), "w+");
        if(!fp) {
            rt = -2;
            break;
        }
        PEM_write_RSAPublicKey(fp, rsa);
        fclose(fp);
        fp = nullptr;

        fp = fopen(prikey_file.c_str(), "w+");
        if(!fp) {
            rt = -3;
            break;
        }
        PEM_write_RSAPrivateKey(fp, rsa, NULL, NULL, 0, NULL, NULL);
        fclose(fp);
        fp = nullptr;
    } while(false);
    if(fp) {
        fclose(fp);
    }
    if(rsa) {
        RSA_free(rsa);
    }
    return rt;
}

RSACipher::ptr RSACipher::Create(const std::string& pubkey_file
                      ,const std::string& prikey_file) {
    FILE* fp = nullptr;
    do {
        RSACipher::ptr rt(new RSACipher);
        fp = fopen(pubkey_file.c_str(), "r+");
        if(!fp) {
            break;
        }
        rt->m_pubkey = PEM_read_RSAPublicKey(fp, NULL, NULL, NULL);
        if(!rt->m_pubkey) {
            break;
        }

        RSA_print_fp(stdout, rt->m_pubkey, 0);
        std::cout << "====" << std::endl;

        std::string tmp;
        tmp.resize(1024);

        int len = 0;
        fseek(fp, 0, 0);
        do {
            len = fread(&tmp[0], 1, tmp.size(), fp);
            if(len > 0) {
                rt->m_pubkeyStr.append(tmp.c_str(), len);
            }
        } while(len > 0);
        fclose(fp);
        fp = nullptr;

        fp = fopen(prikey_file.c_str(), "r+");
        if(!fp) {
            break;
        }
        rt->m_prikey = PEM_read_RSAPrivateKey(fp, NULL, NULL, NULL);
        if(!rt->m_prikey) {
            break;
        }

        RSA_print_fp(stdout, rt->m_prikey, 0);
        std::cout << "====" << std::endl;
        fseek(fp, 0, 0);
        do {
            len = fread(&tmp[0], 1, tmp.size(), fp);
            if(len > 0) {
                rt->m_prikeyStr.append(tmp.c_str(), len);
            }
        } while(len > 0);
        fclose(fp);
        fp = nullptr;
        return rt;
    } while(false);
    if(fp) {
        fclose(fp);
    }
    return nullptr;
}

RSACipher::RSACipher()
    :m_pubkey(nullptr)
    ,m_prikey(nullptr) {
}

RSACipher::~RSACipher() {
    if(m_pubkey) {
        RSA_free(m_pubkey);
    }
    if(m_prikey) {
        RSA_free(m_prikey);
    }
}

int32_t RSACipher::privateEncrypt(const void* from, int flen,
                       void* to, int padding) {
    return RSA_private_encrypt(flen, (const uint8_t*)from
                                ,(uint8_t*)to, m_prikey, padding);
}

int32_t RSACipher::publicEncrypt(const void* from, int flen,
                       void* to, int padding) {
    return RSA_public_encrypt(flen, (const uint8_t*)from
                                ,(uint8_t*)to, m_pubkey, padding);
}

int32_t RSACipher::privateDecrypt(const void* from, int flen,
                       void* to, int padding) {
    return RSA_private_decrypt(flen, (const uint8_t*)from
                                ,(uint8_t*)to, m_prikey, padding);
}

int32_t RSACipher::publicDecrypt(const void* from, int flen,
                       void* to, int padding) {
    return RSA_public_decrypt(flen, (const uint8_t*)from
                                ,(uint8_t*)to, m_pubkey, padding);
}

int32_t RSACipher::privateEncrypt(const void* from, int flen,
                       std::string& to, int padding) {
    //TODO resize
    int32_t len = privateEncrypt(from, flen, &to[0], padding);
    if(len >= 0) {
        to.resize(len);
    }
    return len;
}

int32_t RSACipher::publicEncrypt(const void* from, int flen,
                       std::string& to, int padding) {
    //TODO resize
    int32_t len = publicEncrypt(from, flen, &to[0], padding);
    if(len >= 0) {
        to.resize(len);
    }
    return len;
}

int32_t RSACipher::privateDecrypt(const void* from, int flen,
                       std::string& to, int padding) {
    //TODO resize
    int32_t len = privateDecrypt(from, flen, &to[0], padding);
    if(len >= 0) {
        to.resize(len);
    }
    return len;
}

int32_t RSACipher::publicDecrypt(const void* from, int flen,
                       std::string& to, int padding) {
    //TODO resize
    int32_t len = publicDecrypt(from, flen, &to[0], padding);
    if(len >= 0) {
        to.resize(len);
    }
    return len;
}

int32_t RSACipher::getPubRSASize() {
    if(m_pubkey) {
        return RSA_size(m_pubkey);
    }
    return -1;
}

int32_t RSACipher::getPriRSASize() {
    if(m_prikey) {
        return RSA_size(m_prikey);
    }
    return -1;
}

}