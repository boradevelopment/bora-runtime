
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
