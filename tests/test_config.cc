#include "../chat/config.h"
#include "../chat/log.h"
#include <yaml-cpp/yaml.h>

chat::ConfigVar<int>::ptr g_int_val_config = 
    chat::Config::Lookup("system.port", (int)8080, "system port");

// chat::ConfigVar<float>::ptr g_int_valx_config = 
//     chat::Config::Lookup("system.port", (float)8080, "system port");


chat::ConfigVar<float>::ptr g_float_val_config = 
    chat::Config::Lookup("system.value", (float)10.2f, "system value");

chat::ConfigVar<std::vector<int> >::ptr g_int_vec_val_config = 
    chat::Config::Lookup("system.int_vec", std::vector<int>{1, 2}, "system int vec");

chat::ConfigVar<std::list<int> >::ptr g_int_list_val_config = 
    chat::Config::Lookup("system.int_list", std::list<int>{1, 2}, "system int list");

chat::ConfigVar<std::set<int> >::ptr g_int_set_val_config = 
    chat::Config::Lookup("system.int_set", std::set<int>{1, 2}, "system int set");

chat::ConfigVar<std::unordered_set<int> >::ptr g_int_uset_val_config = 
    chat::Config::Lookup("system.int_uset", std::unordered_set<int>{1, 2}, "system int uset");

chat::ConfigVar<std::map<std::string, int> >::ptr g_str_int_map_val_config = 
    chat::Config::Lookup("system.str_int_map", std::map<std::string, int>{{"d", 2}}, "system str int map");

chat::ConfigVar<std::unordered_map<std::string, int> >::ptr g_str_int_umap_val_config = 
    chat::Config::Lookup("system.str_int_umap", std::unordered_map<std::string, int>{{"c", 2}}, "system str int umap");


void print_yaml(const YAML::Node& node, int level) {
    if (node.IsScalar()) {
        CHAT_LOG_INFO(CHAT_LOG_ROOT()) << std::string(level + 4 , ' ') << node.Scalar() << " - " << node.Type() << " - " << level;
    } else if (node.IsNull()){
        CHAT_LOG_INFO(CHAT_LOG_ROOT()) << std::string(level + 4 , ' ') << "NULL - " << node.Type() << " - " << level;
    } else if (node.IsMap()) {
        for (auto it = node.begin(); it != node.end(); it++) {
            CHAT_LOG_INFO(CHAT_LOG_ROOT()) << std::string(level + 4 , ' ') << it->first << " - " << it->second.Type() << " - " << level;
            print_yaml(it->second, level + 1);
        }
    } else if (node.IsSequence()) {
        for (size_t i = 0; i < node.size(); i++) {
            CHAT_LOG_INFO(CHAT_LOG_ROOT()) << std::string(level + 4 , ' ') << i << " - " << node[i].Type() << " - " << level;
            print_yaml(node[i], level + 1);
        }
    }
}

void test_yaml() {
    YAML::Node root = YAML::LoadFile("/root/serverProjects/cpp_chatroom_server/bin/conf/log.yml");
    print_yaml(root, 0);
    // CHAT_LOG_INFO(CHAT_LOG_ROOT()) << root;
    CHAT_LOG_INFO(CHAT_LOG_ROOT()) << root.Scalar();
}

void test_config() {
    CHAT_LOG_INFO(CHAT_LOG_ROOT()) << "before: " << g_int_val_config->getValue();
    CHAT_LOG_INFO(CHAT_LOG_ROOT()) << "before: " << g_float_val_config->toString();

#define XX(g_var, name, prefix) \
    { \
        auto v = g_var->getValue(); \
        for (auto& i : v) { \
            CHAT_LOG_INFO(CHAT_LOG_ROOT()) << #prefix " " #name ": " << i; \
        } \
        CHAT_LOG_INFO(CHAT_LOG_ROOT()) << #prefix " " #name ": " << g_var->toString(); \
    }
#define XX_M(g_var, name, prefix) \
    { \
        auto& v = g_var->getValue(); \
        for(auto& i : v) { \
            CHAT_LOG_INFO(CHAT_LOG_ROOT()) << #prefix " " #name ": {" \
                    << i.first << " - " << i.second << "}"; \
        } \
        CHAT_LOG_INFO(CHAT_LOG_ROOT()) << #prefix " " #name " yaml: " << g_var->toString(); \
    }

    XX(g_int_vec_val_config, int_vec, before);
    XX(g_int_list_val_config, int_list, before);
    XX(g_int_set_val_config, int_set, before);
    XX(g_int_uset_val_config, int_uset, before);
    XX_M(g_str_int_map_val_config, str_int_map, before);
    XX_M(g_str_int_umap_val_config, str_int_umap, before);
    

    YAML::Node root = YAML::LoadFile("/root/serverProjects/cpp_chatroom_server/bin/conf/log.yml");
    chat::Config::LoadFromYaml(root);
    
    CHAT_LOG_INFO(CHAT_LOG_ROOT()) << "after: " << g_int_val_config->getValue();
    CHAT_LOG_INFO(CHAT_LOG_ROOT()) << "after: " << g_float_val_config->toString();
    
    XX(g_int_vec_val_config, int_vec, after);
    XX(g_int_list_val_config, int_list, after);
    XX(g_int_set_val_config, int_set, after);
    XX(g_int_uset_val_config, int_uset, after);
    XX_M(g_str_int_map_val_config, str_int_map, after);
    XX_M(g_str_int_umap_val_config, str_int_umap, after);

}

class Person{
public:
    std::string m_name;
    int m_age = 0;
    bool m_sex = 0;

    std::string toString() const {
        std::stringstream ss;
        ss << "{Person name=" << m_name
            << " age=" << m_age
            << " sex=" << m_sex
            << "}";
        return ss.str();
    }

    bool operator==(const Person& oth) const  {
        return m_name == oth.m_name && m_age == oth.m_age && m_sex == oth.m_sex;
    }

};

namespace chat {
template<>
class LexicalCast<std::string, Person> {
public:
    Person operator() (const std::string& v) {
        YAML::Node node = YAML::Load(v);
        Person p;
        p.m_name = node["name"].as<std::string>();
        p.m_age = node["age"].as<int>();
        p.m_sex = node["sex"].as<bool>();
        return p;
    }
};
template<>
class LexicalCast<Person, std::string> {
public:
    std::string operator() (const Person& p) {
        YAML::Node node;
        node["name"] = p.m_name;
        node["age"] = p.m_age;
        node["sex"] = p.m_sex;
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

}

chat::ConfigVar<Person>::ptr g_person = 
    chat::Config::Lookup("class.person", Person(), "system person");

chat::ConfigVar<std::map<std::string, Person> >::ptr g_person_map =
    chat::Config::Lookup("class.map", std::map<std::string, Person>(), "system person");

chat::ConfigVar<std::map<std::string, std::vector<Person> > >::ptr g_person_vec_map =
    chat::Config::Lookup("class.vec_map", std::map<std::string, std::vector<Person> >(), "system person");

void test_class() {
    CHAT_LOG_INFO(CHAT_LOG_ROOT()) << "class.person before: " <<  g_person->getValue().toString() << " - " << g_person->toString();

#define XX_PM(g_var, prefix) \
    { \
        auto m = g_person_map->getValue(); \
        for(auto& i : m) { \
            CHAT_LOG_INFO(CHAT_LOG_ROOT()) <<  prefix << ": " << i.first << " - " << i.second.toString(); \
        } \
        CHAT_LOG_INFO(CHAT_LOG_ROOT()) <<  prefix << ": size=" << m.size(); \
    }

    g_person->addListener(10, [] (const Person& old_v, const Person& new_v){
        CHAT_LOG_INFO(CHAT_LOG_ROOT()) << "old_v=" << old_v.toString() << "new_v: " << new_v.toString();
    });

    XX_PM(g_person_map, "class.map before");
    CHAT_LOG_INFO(CHAT_LOG_ROOT()) << "class.vec_map before: " << g_person_vec_map->toString();

    YAML::Node root = YAML::LoadFile("/root/serverProjects/cpp_chatroom_server/bin/conf/log.yml");
    chat::Config::LoadFromYaml(root);

    CHAT_LOG_INFO(CHAT_LOG_ROOT()) << "class.person after: " << g_person->getValue().toString() << " - " << g_person->toString();
    XX_PM(g_person_map, "class.map after");
    CHAT_LOG_INFO(CHAT_LOG_ROOT()) << "class.vec_map after: " <<  g_person_vec_map->toString();

}

void test_log() {
    static chat::Logger::ptr system_log = CHAT_LOG_NAME("system");
    CHAT_LOG_INFO(system_log) << "hello system" << std::endl;
    std::cout << chat::LoggerMgr::GetInstance()->toYamlString() << std::endl;
    
    YAML::Node root = YAML::LoadFile("/root/serverProjects/cpp_chatroom_server/bin/conf/log.yml");
    chat::Config::LoadFromYaml(root);

    std::cout << "--------------" << std::endl;
    std::cout << chat::LoggerMgr::GetInstance()->toYamlString() << std::endl;
    CHAT_LOG_INFO(system_log) << "hello system" << std::endl;

    system_log->setFormatter("%d - %m%n");
    CHAT_LOG_INFO(system_log) << "hello system" << std::endl;

}

int main(int argc, char** argv) {
    // test_yaml();
    // test_config();
    // test_class();
    test_log();
    return 0;
}