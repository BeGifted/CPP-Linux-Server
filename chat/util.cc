#include <execinfo.h>
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
#include <google/protobuf/unknown_field_set.h>
#include "log.h"
#include "fiber.h"
#include "util.h"
#include "worker.h"

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

std::string ToUpper(const std::string& name) {
    std::string rt = name;
    std::transform(rt.begin(), rt.end(), rt.begin(), ::toupper);
    return rt;
}

std::string ToLower(const std::string& name) {
    std::string rt = name;
    std::transform(rt.begin(), rt.end(), rt.begin(), ::tolower);
    return rt;
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

std::string base64decode(const std::string &src, bool url) {
    std::string result;
    result.resize(src.size() * 3 / 4);
    char *writeBuf = &result[0];

    const char* ptr = src.c_str();
    const char* end = ptr + src.size();

    const char c62 = url ? '-' : '+';
    const char c63 = url ? '_' : '/';

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
            } else if(*ptr == c62) {
                val = 62;
            } else if(*ptr == c63) {
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

std::string base64encode(const std::string& data, bool url) {
    return base64encode(data.c_str(), data.size(), url);
}

std::string base64encode(const void* data, size_t len, bool url) {
    const char* base64 = url ?
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"
        : "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
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
    return hexstring_from_data(result.c_str(), MD5_DIGEST_LENGTH);
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

std::string JsonUtil::GetStringValue(const Json::Value& v 
                      ,const std::string& default_value) {
    if(v.isString()) {
        return v.asString();
    } else if(v.isArray()) {
        return ToString(v);
    } else if(v.isObject()) {
        return ToString(v);
    } else {
        return v.asString();
    }
    return default_value;
}

double JsonUtil::GetDoubleValue(const Json::Value& v
                 ,double default_value) {
    if(v.isDouble()) {
        return v.asDouble();
    } else if(v.isString()) {
        return TypeUtil::Atof(v.asString());
    }
    return default_value;
}

int32_t JsonUtil::GetInt32Value(const Json::Value& v
                 ,int32_t default_value) {
    if(v.isInt()) {
        return v.asInt();
    } else if(v.isString()) {
        return TypeUtil::Atoi(v.asString());
    }
    return default_value;
}

uint32_t JsonUtil::GetUint32Value(const Json::Value& v
                   ,uint32_t default_value) {
    if(v.isUInt()) {
        return v.asUInt();
    } else if(v.isString()) {
        return TypeUtil::Atoi(v.asString());
    }
    return default_value;
}

int64_t JsonUtil::GetInt64Value(const Json::Value& v
                 ,int64_t default_value) {
    if(v.isInt64()) {
        return v.asInt64();
    } else if(v.isString()) {
        return TypeUtil::Atoi(v.asString());
    }
    return default_value;
}

uint64_t JsonUtil::GetUint64Value(const Json::Value& v
                   ,uint64_t default_value) {
    if(v.isUInt64()) {
        return v.asUInt64();
    } else if(v.isString()) {
        return TypeUtil::Atoi(v.asString());
    }
    return default_value;
}


std::string JsonUtil::GetString(const Json::Value& json
                      ,const std::string& name
                      ,const std::string& default_value) {
    if(!json.isMember(name)) {
        return default_value;
    }
    return GetStringValue(json[name], default_value);
}

double JsonUtil::GetDouble(const Json::Value& json
                 ,const std::string& name
                 ,double default_value) {
    if(!json.isMember(name)) {
        return default_value;
    }
    return GetDoubleValue(json[name], default_value);
}

int32_t JsonUtil::GetInt32(const Json::Value& json
                 ,const std::string& name
                 ,int32_t default_value) {
    if(!json.isMember(name)) {
        return default_value;
    }
    return GetInt32Value(json[name], default_value);
}

uint32_t JsonUtil::GetUint32(const Json::Value& json
                   ,const std::string& name
                   ,uint32_t default_value) {
    if(!json.isMember(name)) {
        return default_value;
    }
    return GetUint32Value(json[name], default_value);
}

int64_t JsonUtil::GetInt64(const Json::Value& json
                 ,const std::string& name
                 ,int64_t default_value) {
    if(!json.isMember(name)) {
        return default_value;
    }
    return GetInt64Value(json[name], default_value);
}

uint64_t JsonUtil::GetUint64(const Json::Value& json
                   ,const std::string& name
                   ,uint64_t default_value) {
    if(!json.isMember(name)) {
        return default_value;
    }
    return GetUint64Value(json[name], default_value);
}

bool JsonUtil::FromString(Json::Value& json, const std::string& v) {
    Json::Reader reader;
    return reader.parse(v, json);
}

std::string JsonUtil::ToString(const Json::Value& json, bool emit_utf8) {
    Json::StreamWriterBuilder builder;
    builder["commentStyle"] = "None";
    builder["indentation"] = "";
    builder["emitUTF8"] = emit_utf8;
    return Json::writeString(builder, json);
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
        RSACipher::ptr rt = std::make_shared<RSACipher>();
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


static const char uri_chars[256] = {
    /* 0 */
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 1, 1, 0,
    1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 0, 0, 0, 1, 0, 0,
    /* 64 */
    0, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 0, 0, 0, 0, 1,
    0, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,   1, 1, 1, 0, 0, 0, 1, 0,
    /* 128 */
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    /* 192 */
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
};

static const char xdigit_chars[256] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,1,2,3,4,5,6,7,8,9,0,0,0,0,0,0,
    0,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

#define CHAR_IS_UNRESERVED(c)           \
    (uri_chars[(unsigned char)(c)])

//-.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz~
std::string StringUtil::UrlEncode(const std::string& str, bool space_as_plus) {
    static const char *hexdigits = "0123456789ABCDEF";
    std::string* ss = nullptr;
    const char* end = str.c_str() + str.length();
    for(const char* c = str.c_str() ; c < end; ++c) {
        if(!CHAR_IS_UNRESERVED(*c)) {
            if(!ss) {
                ss = new std::string;
                ss->reserve(str.size() * 1.2);
                ss->append(str.c_str(), c - str.c_str());
            }
            if(*c == ' ' && space_as_plus) {
                ss->append(1, '+');
            } else {
                ss->append(1, '%');
                ss->append(1, hexdigits[(uint8_t)*c >> 4]);
                ss->append(1, hexdigits[*c & 0xf]);
            }
        } else if(ss) {
            ss->append(1, *c);
        }
    }
    if(!ss) {
        return str;
    } else {
        std::string rt = *ss;
        delete ss;
        return rt;
    }
}

std::string StringUtil::UrlDecode(const std::string& str, bool space_as_plus) {
    std::string* ss = nullptr;
    const char* end = str.c_str() + str.length();
    for(const char* c = str.c_str(); c < end; ++c) {
        if(*c == '+' && space_as_plus) {
            if(!ss) {
                ss = new std::string;
                ss->append(str.c_str(), c - str.c_str());
            }
            ss->append(1, ' ');
        } else if(*c == '%' && (c + 2) < end
                    && isxdigit(*(c + 1)) && isxdigit(*(c + 2))){
            if(!ss) {
                ss = new std::string;
                ss->append(str.c_str(), c - str.c_str());
            }
            ss->append(1, (char)(xdigit_chars[(int)*(c + 1)] << 4 | xdigit_chars[(int)*(c + 2)]));
            c += 2;
        } else if(ss) {
            ss->append(1, *c);
        }
    }
    if(!ss) {
        return str;
    } else {
        std::string rt = *ss;
        delete ss;
        return rt;
    }
}

std::string StringUtil::Trim(const std::string& str, const std::string& delimit) {
    auto begin = str.find_first_not_of(delimit);
    if(begin == std::string::npos) {
        return "";
    }
    auto end = str.find_last_not_of(delimit);
    return str.substr(begin, end - begin + 1);
}

std::string StringUtil::TrimLeft(const std::string& str, const std::string& delimit) {
    auto begin = str.find_first_not_of(delimit);
    if(begin == std::string::npos) {
        return "";
    }
    return str.substr(begin);
}

std::string StringUtil::TrimRight(const std::string& str, const std::string& delimit) {
    auto end = str.find_last_not_of(delimit);
    if(end == std::string::npos) {
        return "";
    }
    return str.substr(0, end);
}


std::string StringUtil::WStringToString(const std::wstring& ws) {
    std::string str_locale = setlocale(LC_ALL, "");
    const wchar_t* wch_src = ws.c_str();
    size_t n_dest_size = wcstombs(NULL, wch_src, 0) + 1;
    char *ch_dest = new char[n_dest_size];
    memset(ch_dest,0,n_dest_size);
    wcstombs(ch_dest,wch_src,n_dest_size);
    std::string str_result = ch_dest;
    delete []ch_dest;
    setlocale(LC_ALL, str_locale.c_str());
    return str_result;
}

std::wstring StringUtil::StringToWString(const std::string& s) {
    std::string str_locale = setlocale(LC_ALL, "");
    const char* chSrc = s.c_str();
    size_t n_dest_size = mbstowcs(NULL, chSrc, 0) + 1;
    wchar_t* wch_dest = new wchar_t[n_dest_size];
    wmemset(wch_dest, 0, n_dest_size);
    mbstowcs(wch_dest,chSrc,n_dest_size);
    std::wstring wstr_result = wch_dest;
    delete []wch_dest;
    setlocale(LC_ALL, str_locale.c_str());
    return wstr_result;
}


static void serialize_unknowfieldset(const google::protobuf::UnknownFieldSet& ufs, Json::Value& jnode) {
    std::map<int, std::vector<Json::Value> > kvs;
    for(int i = 0; i < ufs.field_count(); ++i) {
        const auto& uf = ufs.field(i);
        switch((int)uf.type()) {
            case google::protobuf::UnknownField::TYPE_VARINT:
                kvs[uf.number()].push_back((Json::Int64)uf.varint());
                break;
            case google::protobuf::UnknownField::TYPE_FIXED32:
                kvs[uf.number()].push_back((Json::UInt)uf.fixed32());
                break;
            case google::protobuf::UnknownField::TYPE_FIXED64:
                kvs[uf.number()].push_back((Json::UInt64)uf.fixed64());
                break;
            case google::protobuf::UnknownField::TYPE_LENGTH_DELIMITED:
                google::protobuf::UnknownFieldSet tmp;
                auto& v = uf.length_delimited();
                if(!v.empty() && tmp.ParseFromString(v)) {
                    Json::Value vv;
                    serialize_unknowfieldset(tmp, vv);
                    kvs[uf.number()].push_back(vv);
                } else {
                    kvs[uf.number()].push_back(v);
                }
                break;
        }
    }

    for(auto& i : kvs) {
        if(i.second.size() > 1) {
            for(auto& n : i.second) {
                jnode[std::to_string(i.first)].append(n);
            }
        } else {
            jnode[std::to_string(i.first)] = i.second[0];
        }
    }
}

static void serialize_message(const google::protobuf::Message& message, Json::Value& jnode) {
    const google::protobuf::Descriptor* descriptor = message.GetDescriptor();
    const google::protobuf::Reflection* reflection = message.GetReflection();

    for(int i = 0; i < descriptor->field_count(); ++i) {
        const google::protobuf::FieldDescriptor* field = descriptor->field(i);

        if(field->is_repeated()) {
            if(!reflection->FieldSize(message, field)) {
                continue;
            }
        } else {
            if(!reflection->HasField(message, field)) {
                continue;
            }
        }

        if(field->is_repeated()) {
            switch(field->cpp_type()) {
#define XX(cpptype, method, valuetype, jsontype) \
                case google::protobuf::FieldDescriptor::CPPTYPE_##cpptype: { \
                    int size = reflection->FieldSize(message, field); \
                    for(int n = 0; n < size; ++n) { \
                        jnode[field->name()].append((jsontype)reflection->GetRepeated##method(message, field, n)); \
                    } \
                    break; \
                }
            XX(INT32, Int32, int32_t, Json::Int);
            XX(UINT32, UInt32, uint32_t, Json::UInt);
            XX(FLOAT, Float, float, double);
            XX(DOUBLE, Double, double, double);
            XX(BOOL, Bool, bool, bool);
            XX(INT64, Int64, int64_t, Json::Int64);
            XX(UINT64, UInt64, uint64_t, Json::UInt64);
#undef XX
                case google::protobuf::FieldDescriptor::CPPTYPE_ENUM: {
                    int size = reflection->FieldSize(message, field);
                    for(int n = 0; n < size; ++n) {
                        jnode[field->name()].append(reflection->GetRepeatedEnum(message, field, n)->number());
                    }
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
                    int size = reflection->FieldSize(message, field);
                    for(int n = 0; n < size; ++n) {
                        jnode[field->name()].append(reflection->GetRepeatedString(message, field, n));
                    }
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {
                    int size = reflection->FieldSize(message, field);
                    for(int n = 0; n < size; ++n) {
                        Json::Value vv;
                        serialize_message(reflection->GetRepeatedMessage(message, field, n), vv);
                        jnode[field->name()].append(vv);
                    }
                    break;
                }
            }
            continue;
        }

        switch(field->cpp_type()) {
#define XX(cpptype, method, valuetype, jsontype) \
            case google::protobuf::FieldDescriptor::CPPTYPE_##cpptype: { \
                jnode[field->name()] = (jsontype)reflection->Get##method(message, field); \
                break; \
            }
            XX(INT32, Int32, int32_t, Json::Int);
            XX(UINT32, UInt32, uint32_t, Json::UInt);
            XX(FLOAT, Float, float, double);
            XX(DOUBLE, Double, double, double);
            XX(BOOL, Bool, bool, bool);
            XX(INT64, Int64, int64_t, Json::Int64);
            XX(UINT64, UInt64, uint64_t, Json::UInt64);
#undef XX
            case google::protobuf::FieldDescriptor::CPPTYPE_ENUM: {
                jnode[field->name()] = reflection->GetEnum(message, field)->number();
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
                jnode[field->name()] = reflection->GetString(message, field);
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {
                serialize_message(reflection->GetMessage(message, field), jnode[field->name()]);
                break;
            }
        }

    }

    const auto& ufs = reflection->GetUnknownFields(message);
    serialize_unknowfieldset(ufs, jnode);
}

std::string PBToJsonString(const google::protobuf::Message& message) {
    Json::Value jnode;
    serialize_message(message, jnode);
    return chat::JsonUtil::ToString(jnode);
}

std::vector<std::string> split(const std::string &str, char delim, size_t max) {
    std::vector<std::string> result;
    if (str.empty()) {
        return result;
    }

    size_t last = 0;
    size_t pos = str.find(delim);
    while (pos != std::string::npos) {
        result.push_back(str.substr(last, pos - last));
        last = pos + 1;
        if (--max == 1)
            break;
        pos = str.find(delim, last);
    }
    result.push_back(str.substr(last));
    return result;
}

std::vector<std::string> split(const std::string &str, const char *delims, size_t max) {
    std::vector<std::string> result;
    if (str.empty()) {
        return result;
    }

    size_t last = 0;
    size_t pos = str.find_first_of(delims);
    while (pos != std::string::npos) {
        result.push_back(str.substr(last, pos - last));
        last = pos + 1;
        if (--max == 1)
            break;
        pos = str.find_first_of(delims, last);
    }
    result.push_back(str.substr(last));
    return result;
}

std::string GetHostName() {
    std::shared_ptr<char> host(new char[512], chat::delete_array<char>);
    memset(host.get(), 0, 512);
    gethostname(host.get(), 511);
    return host.get();
}

in_addr_t GetIPv4Inet() {
    struct ifaddrs* ifas = nullptr;
    struct ifaddrs* ifa = nullptr;

    in_addr_t localhost = inet_addr("127.0.0.1");
    if(getifaddrs(&ifas)) {
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


TimeCalc::TimeCalc()
    :m_time(chat::GetCurrentUs()) {
}

uint64_t TimeCalc::elapse() const {
    return chat::GetCurrentUs() - m_time;
}

void TimeCalc::tick(const std::string& name) {
    m_timeLine.push_back(std::make_pair(name, elapse()));
}

std::string TimeCalc::toString() const {
    std::stringstream ss;
    uint64_t last = 0;
    for(size_t i = 0; i < m_timeLine.size(); ++i) {
        ss << "(" << m_timeLine[i].first << ":" << (m_timeLine[i].second - last) << ")";
        last = m_timeLine[i].second;
    }
    return ss.str();
}

Tracker::Tracker()
    :m_datas(nullptr)
    ,m_interval(60) {
}

Tracker::~Tracker() {
    if(m_datas) {
        delete[] m_datas;
    }
}

void Tracker::init() {
    if(m_datas) {
        delete[] m_datas;
        m_datas = nullptr;
    }
    m_dimCount = m_dims.size() * m_interval;
    m_datas = new int64_t[m_dimCount]();
}

void Tracker::start() {
    if(m_timer) {
        return;
    }
    init();
    auto iom = chat::WorkerMgr::GetInstance()->getAsIOManager("timer");
    if(iom) {
        m_timer = iom->addTimer(100, std::bind(&Tracker::onTimer, shared_from_this()), true);
    } else {
        CHAT_LOG_INFO(g_logger) << "Woker timer not exists";
        chat::IOManager::GetThis()->addTimer(100, std::bind(&Tracker::onTimer, shared_from_this()), true);
    }
}

void Tracker::stop() {
    if(m_timer) {
        m_timer->cancel();
        m_timer = nullptr;
    }
}

int64_t Tracker::inc(uint32_t key, int64_t v) {
    if(!m_datas) {
        return 0;
    }
    time_t now = time(0);
    auto idx = key * m_interval + now % m_interval;
    if(idx < m_dimCount) {
        return chat::Atomic::addFetch(m_datas[idx], v);
    }
    return 0;
}

int64_t Tracker::dec(uint32_t key, int64_t v) {
    if(!m_datas) {
        return 0;
    }
    time_t now = time(0);
    auto idx = key * m_interval + now % m_interval;
    if(idx < m_dimCount) {
        return chat::Atomic::subFetch(m_datas[idx], v);
    }
    return 0;
}

int64_t Tracker::privateInc(uint32_t key, int64_t v) {
    if(!m_datas) {
        return 0;
    }
    if(key < m_dimCount) {
        return chat::Atomic::subFetch(m_datas[key], v);
    }
    return 0;
}

int64_t Tracker::privateDec(uint32_t key, int64_t v) {
    if(!m_datas) {
        return 0;
    }
    if(key < m_dimCount) {
        return chat::Atomic::subFetch(m_datas[key], v);
    }
    return 0;
}

void Tracker::toData(std::map<std::string, int64_t>& data, const std::set<uint32_t>& times) {
    time_t now = time(0) - 1;
    for(uint32_t i = 0; i < m_dims.size(); ++i) {
        int64_t total = 0;
        if(m_dims[i].empty()) {
            continue;
        }
        for(uint32_t n = 0; n < m_interval; ++n) {
            int32_t cur = i * m_interval + (now - n) % m_interval;
            total += m_datas[cur];

            if(times.count(n + 1)) {
                data[m_dims[i] + "." + std::to_string(n + 1)] = total;
            }
        }
        if(times.empty()) {
            data[m_dims[i] + "." + std::to_string(m_interval)] = total;
        }
    }
}

void Tracker::toDurationData(std::map<std::string, int64_t>& data, uint32_t duration, bool with_time) {
    time_t now = time(0) - 1;
    if(duration > m_interval) {
        duration = m_interval;
    }
    for(uint32_t i = 0; i < m_dims.size(); ++i) {
        int64_t total = 0;
        if(m_dims[i].empty()) {
            continue;
        }
        for(uint32_t n = 0; n < duration; ++n) {
            int32_t cur = i * m_interval + (now - n) % m_interval;
            total += m_datas[cur];
        }
        if(with_time) {
            data[m_dims[i] + "." + std::to_string(duration)] = total;
        } else {
            data[m_dims[i]] = total;
        }
    }
}

void Tracker::toTimeData(std::map<std::string, int64_t>& data, uint32_t ts, bool with_time) {
    if(ts >= m_interval) {
        return;
    }
    for(uint32_t i = 0; i < m_dims.size(); ++i) {
        if(m_dims[i].empty()) {
            continue;
        }
        int32_t cur = i * m_interval + ts;
        if(with_time) {
            data[m_dims[i] + "." + std::to_string(ts)] = m_datas[cur];
        } else {
            data[m_dims[i]] = m_datas[cur];
        }
    }
}

void Tracker::addDim(uint32_t key, const std::string& name) {
    if(m_timer) {
        CHAT_LOG_ERROR(g_logger) << "Tracker is running, addDim key=" << key << " name=" << name;
        return;
    }
    if(key >= m_dims.size()) {
        m_dims.resize(key + 1);
    }
    m_dims[key] = name;
}

void Tracker::onTimer() {
    time_t now = time(0);
    int offset = (now + 1) % m_interval;

    for(uint32_t i = 0; i < m_dims.size(); ++i) {
        auto idx = i * m_interval + offset;
        if(idx < m_dimCount) {
            m_datas[idx] = 0;
        }
    }
}

TrackerManager::TrackerManager()
    :m_interval(60) {
}

void TrackerManager::addDim(uint32_t key, const std::string& name) {
    RWMutexType::WriteLock lock(m_mutex);
    if(key >= m_dims.size()) {
        m_dims.resize(key + 1);
    }
    m_dims[key] = name;
}

int64_t TrackerManager::inc(const std::string& name, uint32_t key, int64_t v) {
    m_totalTracker->inc(key, v);
    auto info = getOrCreate(name);
    return info->inc(key, v);
}

int64_t TrackerManager::dec(const std::string& name, uint32_t key, int64_t v) {
    m_totalTracker->dec(key, v);
    auto info = getOrCreate(name);
    return info->dec(key, v);
}

Tracker::ptr TrackerManager::getOrCreate(const std::string& name) {
    do {
        RWMutexType::ReadLock lock(m_mutex);
        auto it = m_datas.find(name);
        if(it != m_datas.end()) {
            return it->second;
        }
    } while(0);
    RWMutexType::WriteLock lock(m_mutex);
    auto it = m_datas.find(name);
    if(it != m_datas.end()) {
        return it->second;
    }

    Tracker::ptr rt = std::make_shared<Tracker>();
    rt->m_dims = m_dims;
    rt->m_interval = m_interval;
    rt->m_name = name;
    rt->start();
    m_datas[name] = rt;
    lock.unlock();
    return rt;
}

Tracker::ptr TrackerManager::get(const std::string& name) {
    RWMutexType::ReadLock lock(m_mutex);
    auto it = m_datas.find(name);
    return it == m_datas.end() ? nullptr : it->second;
}

std::string TrackerManager::toString(uint32_t duration) {
    std::stringstream ss;
    RWMutexType::ReadLock lock(m_mutex);
    auto datas = m_datas;
    lock.unlock();

    ss << "<TrackerManager count=" << datas.size() << ">" << std::endl;
    if(m_totalTracker) {
        std::map<std::string, int64_t> data;
        m_totalTracker->toDurationData(data, duration);

        ss << "[Tracker name=" << m_totalTracker->m_name << "]" << std::endl;
        for(auto& n : data) {
            ss << std::setw(40) << std::right << n.first << ": " << n.second << std::endl;
        }
    }
    for(auto& i : datas) {
        std::map<std::string, int64_t> data;
        i.second->toDurationData(data, duration);

        ss << "[Tracker name=" << i.first << "]" << std::endl;
        for(auto& n : data) {
            ss << std::setw(40) << std::right << n.first << ": " << n.second << std::endl;
        }
    }
    return ss.str();
}

void TrackerManager::start() {
    if(m_timer) {
        return;
    }

    m_totalTracker = std::make_shared<Tracker>();
    m_totalTracker->m_dims = m_dims;
    m_totalTracker->m_interval = 1;
    m_totalTracker->m_name = "total";
    m_totalTracker->init();

    m_timer = chat::IOManager::GetThis()->addTimer(1000 * 60 * 60 * 8, [this](){
            RWMutexType::WriteLock lock(m_mutex);
            auto datas = m_datas;
            m_datas.clear();
            lock.unlock();

            for(auto& i : datas) {
                i.second->stop();
            }
    }, true);
}


}