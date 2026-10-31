// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: HookRegister.h
 * Purpose: ?
*/
#pragma once
#include <vector>
#include <functional>

class HookRegistry {
public:
    using RegisterCallback = void (*)();

    static HookRegistry& instance() {
        static HookRegistry inst;
        return inst;
    }

    void addClient(RegisterCallback cb) {
        callbacksClient_.push_back(cb);
    }

    void addServer(RegisterCallback cb) {
        callbacksServer_.push_back(cb);
    }

    void registerClient() {
        for (auto cb : callbacksClient_) {
            cb();
        }
    }

    void registerServer() {
        for (auto cb : callbacksServer_) {
            cb();
        }
    }

private:
    std::vector<RegisterCallback> callbacksServer_;
    std::vector<RegisterCallback> callbacksClient_;
};

class HookAutoRegisterServer {
public:
    HookAutoRegisterServer(HookRegistry::RegisterCallback cb) {
        HookRegistry::instance().addServer(cb);
    }
};

class HookAutoRegisterClient {
public:
    HookAutoRegisterClient(HookRegistry::RegisterCallback cb) {
        HookRegistry::instance().addClient(cb);
    }
};


#define SUPERVISOR_REGISTER_SERVER(func_name) \
    static void func_name(); \
    static HookAutoRegisterServer CONCAT(auto_reg_, __LINE__)(func_name); \
    static void func_name()

#define SUPERVISOR_REGISTER_CLIENT(func_name) \
    static void func_name(); \
    static HookAutoRegisterClient CONCAT(auto_reg_, __LINE__)(func_name); \
    static void func_name()

class SupervisorHookRegister
{
public:
    static void registerClient(){
        HookRegistry::instance().registerClient();
    };

    static void registerServer(){
        HookRegistry::instance().registerServer();
    };
};
