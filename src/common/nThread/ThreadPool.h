// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: ThreadPool.h
 * Purpose: A literal thread pool
*/
#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <memory>
#include <stdexcept>

#include "ThreadUtilities.h"

class ThreadPool {
public:
    // Constructor initializes the specified number of threads
    explicit ThreadPool(size_t threads) : stop_(false) {
        for(size_t i = 0; i < threads; ++i) {
            workers_.emplace_back([this, i] {
                bora::thread::nameThread(std::format("tp{}", i).c_str());
                while(true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex_);
                        this->cv_.wait(lock, [this]{
                            return this->stop_ || !this->tasks_.empty();
                        });

                        if(this->stop_ && this->tasks_.empty())
                            return;

                        task = std::move(this->tasks_.front());
                        this->tasks_.pop();
                    }
                    // Execute the task outside the lock
                    task();
                }
            });
        }
    }

    // Enqueue function to add tasks to the pool
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type> {

        using return_type = typename std::invoke_result<F, Args...>::type;

        auto boundTask = [
            func = std::forward<F>(f),
            ...args = std::forward<Args>(args)
        ]() mutable -> return_type {
            return std::invoke(func, std::move(args)...);
        };

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::move(boundTask)
        );

        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);

            if(stop_)
                throw std::runtime_error("enqueue on stopped ThreadPool");

            tasks_.emplace([task]()
            {
                (*task)();
            });
        }
        cv_.notify_one();
        return res;
    }

    // Destructor joins all threads cleanly
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for(std::thread &worker: workers_) {
            if(worker.joinable())
                worker.join();
        }
    }

private:
    std::mutex queue_mutex_;
    std::condition_variable cv_;
    bool stop_;
    std::queue<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;
};
