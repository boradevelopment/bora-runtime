#include "thread.h"
#include <ranges>
#include "nThread/ThreadUtilities.h"

// This is executed every WASM instruction tick for all threads.
static wasmtime_error_t* epoch_callback(
    wasmtime_context_t* context,
    void* data,
    uint64_t* epoch_deadline_delta,
    wasmtime_update_deadline_kind_t* update_kind
) {
    if (auto* controller = static_cast<BoraThreadInterruptState*>(data))
    {
        if (!controller) return nullptr;
        std::unique_lock<std::mutex> lock(controller->mtx);

        // If a pause/interrupt was requested, block right here inside the callback
        while (controller->paused && !controller->should_stop) {
            // Set next deadline far out or keep it ticking while waiting
            *epoch_deadline_delta = 1;
            *update_kind = WASMTIME_UPDATE_DEADLINE_CONTINUE;

            // This puts the worker thread to sleep until controller->cv.notify_all() is called
            controller->cv.wait(lock);
        }

        // If the thread was told to stop completely during the pause
        if (controller->should_stop) {
            // Returning a trap error safely unwinds and terminates the Wasm execution
            return wasmtime_error_new("Thread was killed");
        }

        *epoch_deadline_delta = 1;
        *update_kind = WASMTIME_UPDATE_DEADLINE_CONTINUE;
    }

    return nullptr; // Return no error
}


void BoraThreadSymbols::CreateMainThread(const WasmRuntimeModule* module)
{
    ++id_counter;
    int id = id_counter;
    threads[id] = std::move(std::make_unique<BoraModuleThread>(id,module, &module->mainThread));
    auto thread = threads[id].get();
    thread->isMain = true;
    thread->interrupt_state.instance = &module->mainThread;
    wasmtime_store_epoch_deadline_callback(module->mainThread.store, epoch_callback, &thread->interrupt_state, nullptr);
    wasmtime_context_set_epoch_deadline(module->mainThread.context, 1);
}

void BoraThreadSymbols::CreateEpochThread()
{
    epochThreadActive = true;
    epochThread = std::thread([]()
    {
        while (epochThreadActive && WasmGlobal::getInstance()->getEngine() != nullptr)
        {
            wasmtime_engine_increment_epoch(WasmGlobal::getInstance()->getEngine());
        }
    });

    bora::thread::nameThread(epochThread,  "EPOCH");
}

void BoraThreadSymbols::ShutdownAllThreads()
{
    for (auto& thread : threads | std::views::values)
    {
        if (!thread->isMain)
        { // In most cases, the thread is detached.
            if (thread->state != BoraThreadState::Finished)
            {
                thread->interrupt_state.should_stop = true;
                thread->interrupt_state.sleepCV.notify_all();
                thread->interrupt_state.cv.notify_all();
            }
            if (thread->thread.joinable())
            {
                thread->thread.join();
            }
        }
    }
    threads.clear();
    epochThreadActive = false;
    epochThread.join();
}

int BoraThreadSymbols::create_thread(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance, wasmtime_caller_t* moduleCaller,  u64 func_ptr,  u64 argData, u64 argSize){
    auto engine = WasmGlobal::getInstance()->getEngine();
    ++id_counter;
    int id = id_counter;
    wasmtime_store_t* store = wasmtime_store_new(engine, nullptr, nullptr);
    wasmtime_store_epoch_deadline_callback(store, stub_epoch_callback, nullptr, nullptr); // stub for now
    auto context = wasmtime_store_context(store);
    auto stackOffset = WasmTools::allocateMemoryToWasm(threadInstance, module, WasmGlobal::getCommonStackSize());
    // Initialize dependency thread instances - this will be managed by the thread.
    std::list<const WasmThreadInstance*> dependencyInstances;
    for (auto dependency : WasmGlobal::getInstance()->getLibraries())
    {
        auto dependencyInstance = WasmGlobal::getInstance()->registerRuntimeLibraryForThread(store, context, dependency, stackOffset);
        dependencyInstances.push_back(dependencyInstance);
    }
    const WasmThreadInstance* thread = WasmRunner::createThreadInstance(WasmGlobal::getInstance(), store, context, module, stackOffset);
    if (!thread)
    {
        return -1;
    }
    sUniquePointer<BoraModuleThread> BoraModuleThread = std::make_unique<struct BoraModuleThread>(id, module, thread);
    wasmtime_store_epoch_deadline_callback(store, epoch_callback, &BoraModuleThread->interrupt_state, nullptr);
    wasmtime_context_set_epoch_deadline(thread->context, 1);
    auto moduleRef = BoraModuleThread.get();
    {
        std::lock_guard<std::mutex> lock(mutex);
        threads[id_counter] = std::move(BoraModuleThread);
        threads[id_counter]->thread = std::thread([module, func_ptr, argData, argSize, id, thread, store, moduleRef, dependencyInstances, stackOffset]()
        {
            moduleRef->state = BoraThreadState::Running;
            wasmtime_error_t* error = nullptr;
            wasm_trap_t* trap = nullptr;
            WasmTools::CallByIndex<void>(thread, module, func_ptr, &trap, &error, argData, argSize);
            wasmtime_store_epoch_deadline_callback(store, stub_epoch_callback, nullptr, nullptr); // fix for epoch callback bugs
            std::string logname = module->appId + " Thread " + std::to_string(id);
            WasmTools::logandDeleteWASMFail(error, trap, logname);
            WasmTools::freeMemoryToWasm(thread, module, stackOffset);
            for (auto depInstance : dependencyInstances)
            {
                delete depInstance;
            }
            delete thread;
            moduleRef->state = BoraThreadState::Finished;
        });
    }

    return id_counter;
}

int BoraThreadSymbols::join_thread(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance, wasmtime_caller_t* moduleCaller,  int thread_id) {
    std::thread t;
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = threads.find(thread_id);
        if (it == threads.end()) return -1; // Thread not found or already joined

        if (it->second->state == BoraThreadState::Interrupted)
        {

        }

        t = std::move(it->second->thread);
        if (t.joinable()) { // fix to prevent heap use after free because it gets erased.
            t.join();
        }
        threads.erase(it);
    }


    return 0; // Success
}

void BoraThreadSymbols::interrupt_thread(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance,
    wasmtime_caller_t* moduleCaller, int thread_id)
{
    std::lock_guard<std::mutex> lock(mutex);
    auto it = threads.find(thread_id);
    if (it == threads.end() || it->second->state != BoraThreadState::Running) return; // Thread not found or already joined

    it->second->state = BoraThreadState::Interrupted;
    it->second->interrupt_state.paused = true;
    it->second->interrupt_state.sleepCV.notify_all();

    if (it->second->name)
    {
        LOG_INFO_VERBOSE(LogManager::instance().getLogger("bora.thread"), 1) << "Thread " << thread_id << "(" << it->second->name << ")" << " interrupted";
    } else
    {
        LOG_INFO_VERBOSE(LogManager::instance().getLogger("bora.thread"), 1) << "Thread " << thread_id << " interrupted";
    }
}

void BoraThreadSymbols::resume_thread(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance,
    wasmtime_caller_t* moduleCaller, int thread_id)
{
    std::lock_guard<std::mutex> lock(mutex);
    auto it = threads.find(thread_id);
    if (it == threads.end() || it->second->state != BoraThreadState::Interrupted) return; // Thread not found or already joined

    it->second->interrupt_state.paused = false;
    it->second->interrupt_state.cv.notify_all();

    if (it->second->name)
    {
        LOG_INFO_VERBOSE(LogManager::instance().getLogger("bora.thread"), 1) << "Thread " << thread_id << "(" << it->second->name << ")" << " resumed";
    } else
    {
        LOG_INFO_VERBOSE(LogManager::instance().getLogger("bora.thread"), 1) << "Thread " << thread_id << " resumed";
    }
}

void BoraThreadSymbols::kill_thread(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance,
    wasmtime_caller_t* moduleCaller, int thread_id)
{
    std::lock_guard<std::mutex> lock(mutex);
    auto it = threads.find(thread_id);
    if (it == threads.end() || it->second->state != BoraThreadState::Running || it->second->isMain) return; // Thread not found or already joined

    it->second->interrupt_state.should_stop = true;
    it->second->interrupt_state.sleepCV.notify_all();
    it->second->interrupt_state.cv.notify_all();

    if (it->second->thread.joinable())
    {
        it->second->thread.join();
    }

    threads.erase(thread_id);
}

void BoraThreadSymbols::detach_thread(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance,
    wasmtime_caller_t* moduleCaller, int thread_id)
{
    std::lock_guard<std::mutex> lock(mutex);
    auto it = threads.find(thread_id);
    if (it == threads.end() || it->second->state != BoraThreadState::Running || it->second->isMain) return; // Thread not found or already joined

    if (it->second->thread.joinable())
    {
       it->second->thread.detach();
    }
}

void BoraThreadSymbols::set_thread_name(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance,
                                        wasmtime_caller_t* moduleCaller, int thread_id, u64 namePtr)
{
    std::lock_guard<std::mutex> lock(mutex);
    auto it = threads.find(thread_id);
    if (it == threads.end() || it->second->state != BoraThreadState::Running || it->second->isMain) return; // Thread not found or already joined

    auto name = WasmTools::fromWASM<const char*>(it->second->instance, it->second->module, namePtr);
    if (!name) return;
    it->second->name = name;
    bora::thread::nameThread(it->second->thread, "Supervisor Client Thread");
}

u64 BoraThreadSymbols::get_thread_name(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance,
    wasmtime_caller_t* moduleCaller, int thread_id)
{
    std::lock_guard<std::mutex> lock(mutex);
    auto it = threads.find(thread_id);
    if (it == threads.end() || it->second->isMain) return 0; // Thread not found or already joined

    return WasmTools::toWASM(it->second->instance, it->second->module, (void*)it->second->name);
}

i64 BoraThreadSymbols::get_thread_id(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance,
    wasmtime_caller_t* moduleCaller)
{
    std::lock_guard<std::mutex> lock(mutex);
    auto it = std::ranges::find_if(threads, [&](const auto& pair) {
        const auto& thread_ptr = pair.second;
        if (!thread_ptr) return false;

        return thread_ptr->instance == threadInstance;
    });

    if (it != threads.end()) {
        return it->second->id; // Return the thread ID from the matched context
    }

    return 0; // Not found
}

void BoraThreadSymbols::thread_sleep_yield(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance,
    wasmtime_caller_t* moduleCaller)
{
    std::this_thread::yield();
}

void BoraThreadSymbols::thread_sleep_for(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance,
                                         wasmtime_caller_t* moduleCaller, u64 time)
{
    std::lock_guard<std::mutex> lock(mutex);
    auto it = std::ranges::find_if(threads, [&](const auto& pair) {
      const auto& thread_ptr = pair.second;
      if (!thread_ptr) return false;

      return thread_ptr->instance == threadInstance;
    });

    auto controller = &it->second->interrupt_state;
    if (!controller) {
        std::this_thread::sleep_for(std::chrono::milliseconds(time));
        return;
    }

    std::unique_lock<std::mutex> lock1(controller->mtx);
    controller->sleepCV.wait_for(lock1, std::chrono::milliseconds(time), [controller] {
        return controller->should_stop || controller->paused;
    });
}

void BoraThreadSymbols::thread_sleep_until(const WasmRuntimeModule* module, const WasmThreadInstance* threadInstance,
    wasmtime_caller_t* moduleCaller, u64 time)
{
    std::lock_guard<std::mutex> lock(mutex);
    auto target_time = std::chrono::steady_clock::time_point(std::chrono::milliseconds(time));
    auto it = std::ranges::find_if(threads, [&](const auto& pair) {
      const auto& thread_ptr = pair.second;
      if (!thread_ptr) return false;

      return thread_ptr->instance == threadInstance;
    });

    auto controller = &it->second->interrupt_state;
    if (!controller) {
        std::this_thread::sleep_for(std::chrono::milliseconds(time));
        return;
    }

    std::unique_lock<std::mutex> lock1(controller->mtx);
    controller->sleepCV.wait_until(lock1, target_time, [controller] {
        return controller->should_stop || controller->paused;
    });
}
