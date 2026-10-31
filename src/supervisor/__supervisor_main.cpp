// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
// Main Function for a regular runtime process
#include "__supervisor_main.h"

#include <csignal>
#include <thread>
#include <sys/stat.h>

#include "SupervisorGlobal.h"
#include "app/IAppPermissions.h"
#include "networking/SupervisorSocketManager.h"
#include "networking/supervisor/hooks/SupervisorHookRegister.h"
#include "nNetworking/DomainSocketFactory.h"
#include "nNetworking/IDomainSocket.h"


void signal_handler(int signum) {
    SupervisorGlobal::instance().keep_running = false;
}

inline std::string getCurrentDateString() {
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
    return oss.str();
}

int
supervisor_main(int argc, char *argv[]){
    auto rootLogger = LogManager::instance().getLogger("root");
    rootLogger->clearAppenders();
    // should be somewhere good for logging, the file format will be bora-supervisor-[date]
    // for now, we are using linux so somewhere good for logging files
    // linux only for now
    std::string logDir = "/var/log/bora";
    std::filesystem::create_directories(logDir);

    auto path = std::format("/var/log/bora/bora-supervisor-{}.log", getCurrentDateString());
    rootLogger->addAppender(std::make_shared<FileAppender>(path, false));

    std::signal(SIGINT, signal_handler);
    SupervisorHookRegister::registerServer();

    SupervisorGlobal::instance().getSocketManager()->open();
    while (SupervisorGlobal::instance().keep_running)
    {
        std::this_thread::yield();
    }
    SupervisorGlobal::instance().getSocketManager()->close();
    return 0;
}
