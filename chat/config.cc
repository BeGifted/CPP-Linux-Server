#include "config.h"
#include "env.h"
#include "log.h"
#include "mutex.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace chat {

static chat::Logger::ptr g_logger = CHAT_LOG_NAME("system");

ConfigVarBase::ptr Config::LookupBase(const std::string& name) {
    RWMutexType::ReadLock lock(GetMutex());
    auto it = GetDatas().find(name);
    return it == GetDatas().end() ? nullptr : it->second;
}

static void ListAllMember(const std::string& prefix,
                            const YAML::Node& node,
                            std::list<std::pair<std::string, const YAML::Node> >& output) {
    if (prefix.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") != std::string::npos){
        CHAT_LOG_ERROR(CHAT_LOG_ROOT()) << "Config Invalid name: " << prefix << " : " << node;
        return;
    }
    output.push_back(std::make_pair(prefix, node));
    if (node.IsMap()) {
        for (auto it = node.begin(); it != node.end(); it++) {
            ListAllMember(prefix.empty() ? it->first.Scalar() : prefix + "." + it->first.Scalar(), it->second, output);
        }
    }
}

void Config::LoadFromYaml(const YAML::Node& root) {
    std::list<std::pair<std::string, const YAML::Node> > all_nodes;
    ListAllMember("", root, all_nodes);

    for (auto& i : all_nodes) {
        std::string key = i.first;
        if (key.empty()) continue;

        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        ConfigVarBase::ptr var = LookupBase(key);

        if (var) {
            if (i.second.IsScalar()) {
                var->fromString(i.second.Scalar());
            } else {
                std::stringstream ss;
                ss << i.second;
                var->fromString(ss.str());
            }
        }
    }
}

static std::map<std::string, uint64_t> s_file2modifytime;

void Config::LoadFromConfDir(const std::string& path, bool force) {
    std::string absoulte_path = chat::EnvMgr::GetInstance()->getAbsolutePath(path);
    std::vector<std::string> files;
    FSUtil::ListAllFile(files, absoulte_path, ".yml");

    for(auto& i : files) {
        {  //时间变了 内容没变 需要通过md5去重
            struct stat st;
            lstat(i.c_str(), &st);
            RWMutexType::ReadLock lock(GetMutex());
            if (!force && s_file2modifytime[i] == (uint64_t)st.st_mtime) {
                continue;
            }
            s_file2modifytime[i] = st.st_mtime;
        }
        try {
            YAML::Node root = YAML::LoadFile(i);
            LoadFromYaml(root);
            CHAT_LOG_INFO(g_logger) << "LoadConfFile file=" << i << " ok";
        } catch (...) {
            CHAT_LOG_ERROR(g_logger) << "LoadConfFile file=" << i << " failed";
        }
    }
}

void Config::Visit(std::function<void(ConfigVarBase::ptr)> cb) {
    RWMutexType::ReadLock lock(GetMutex());
    ConfigVarMap& m = GetDatas();
    for (auto it = m.begin(); it != m.end(); it++) {
        cb(it->second);
    }
}


}