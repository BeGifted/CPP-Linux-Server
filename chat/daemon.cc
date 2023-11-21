#include "daemon.h"
#include "log.h"
#include "config.h"
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace chat {

static chat::Logger::ptr g_logger = CHAT_LOG_NAME("system");
static chat::ConfigVar<uint32_t>::ptr g_daemon_restart_interval
    = chat::Config::Lookup("daemon.restart_interval", (uint32_t)5, "daemon restart interval");

std::string ProcessInfo::toString() const {
    std::stringstream ss;
    ss << "[ProcessInfo parent_id=" << parent_id
       << " main_id=" << main_id
       << " parent_start_time=" << chat::Time2Str(parent_start_time)
       << " main_start_time=" << chat::Time2Str(main_start_time)
       << " restart_count=" << restart_count << "]";
    return ss.str();
}

static int real_start(int argc, char** argv, std::function<int(int argc, char** argv)> main_cb) {
    ProcessInfoMgr::GetInstance()->main_id = getpid();
    ProcessInfoMgr::GetInstance()->main_start_time = time(0);
    return main_cb(argc, argv);
}

static int real_daemon(int argc, char** argv, std::function<int(int argc, char** argv)> main_cb) {
    //nochdir：为零时，当前目录变为根目录，否则不变；
    //noclose：为零时，标准输入、标准输出和错误输出重导向为/dev/null，也就是不输出任何信息，否则照样输出。
    daemon(1, 0);
    ProcessInfoMgr::GetInstance()->parent_id = getpid();
    ProcessInfoMgr::GetInstance()->parent_start_time = time(0);
    while(true) {
        pid_t pid = fork();  //父子进程并行执行相同的代码
        if (pid == 0) {
            //子进程返回
            ProcessInfoMgr::GetInstance()->main_id = getpid();
            ProcessInfoMgr::GetInstance()->main_start_time = time(0);
            CHAT_LOG_INFO(g_logger) << "process start pid=" << getpid();
            return real_start(argc, argv, main_cb);
        } else if (pid < 0) {
            CHAT_LOG_ERROR(g_logger) << "fork fail return=" << pid
                << " errno=" << errno << " errstr=" << strerror(errno);
            return -1;
        } else {
            //父进程返回
            int status = 0;
            waitpid(pid, &status, 0);  //等待子进程退出
            if (status) {
                if(status == 9) {
                    CHAT_LOG_INFO(g_logger) << "killed";
                    break;
                } else {
                    CHAT_LOG_ERROR(g_logger) << "child crash pid=" << pid
                        << " status=" << status;
                }
            } else {
                CHAT_LOG_INFO(g_logger) << "child finished pid=" << pid;
                break;
            }
            ProcessInfoMgr::GetInstance()->restart_count += 1;
            sleep(g_daemon_restart_interval->getValue());
        }
    }
    return 0;
}

int start_daemon(int argc, char** argv
                 , std::function<int(int argc, char** argv)> main_cb
                 , bool is_daemon) {
    if (!is_daemon) {
        ProcessInfoMgr::GetInstance()->parent_id = getpid();
        ProcessInfoMgr::GetInstance()->parent_start_time = time(0);
        return real_start(argc, argv, main_cb);
    }
    return real_daemon(argc, argv, main_cb);
}

}