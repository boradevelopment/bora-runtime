
#pragma once
#include <mutex>
#include <optional>
#include <vector>

template<typename T>
class MutexableVector {
public:
    using value_type = T;

    void push_back(T&& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_.push_back(std::move(value));
    }

    void push_back(const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_.push_back(value);
    }

    std::optional<T> pop_back() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (data_.empty()) return std::nullopt;
        T value = std::move(data_.back());
        data_.pop_back();
        return value;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        data_.clear();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return data_.size();
    }

    // Safely get a copy of the current vector (for iteration outside the lock)
    std::vector<T> snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return data_;
    }

    // Scoped access to allow custom operations
    template<typename Func>
    void access(Func&& f) {
        std::lock_guard<std::mutex> lock(mutex_);
        f(data_);
    }

private:
    mutable std::mutex mutex_;
    std::vector<T> data_;
};

/*
 *
 */
template<typename T>
class ThreadLocalCommandStream {
public:
    using value_type = T;

    // Single thread's local storage segment
    struct ThreadChunk {
        std::vector<T> items;
    };

    ThreadLocalCommandStream() = default;

    // Zero-lock push operations
    void push_back(const T& value) {
        GetLocalChunk().items.push_back(value);
    }

    void push_back(T&& value) {
        GetLocalChunk().items.push_back(std::move(value));
    }

    // Pop from current calling thread's buffer
    std::optional<T> pop_back() {
        auto& chunk = GetLocalChunk();
        if (chunk.items.empty()) return std::nullopt;

        T val = std::move(chunk.items.back());
        chunk.items.pop_back();
        return val;
    }

    // Total size aggregated across all active threads
    std::size_t size() const {
        std::lock_guard<std::mutex> lock(registryMutex_);
        std::size_t total = 0;
        for (const auto* chunk : registeredChunks_) {
            if (chunk) total += chunk->items.size();
        }
        return total;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(registryMutex_);
        for (auto* chunk : registeredChunks_) {
            if (chunk) chunk->items.clear();
        }
    }

    // Host-side batch processing without vector copying by sending the vector directly to the function
    template<typename Func>
    void access(Func&& f) {
        std::lock_guard<std::mutex> lock(registryMutex_);
        for (auto* chunk : registeredChunks_) {
            if (chunk && !chunk->items.empty()) {
                f(chunk->items);
            }
        }
    }

private:
    ThreadChunk& GetLocalChunk() {
        thread_local ThreadChunk* localPtr = nullptr;
        thread_local ThreadChunk localChunk;

        if (localPtr == nullptr) {
            localPtr = &localChunk;
            std::lock_guard<std::mutex> lock(registryMutex_);
            registeredChunks_.push_back(localPtr);
        }
        return localChunk;
    }

    mutable std::mutex registryMutex_;
    std::vector<ThreadChunk*> registeredChunks_;
};