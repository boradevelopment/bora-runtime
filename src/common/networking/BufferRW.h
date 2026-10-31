// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: BufferRW.h
 * Purpose: Helps read and write a buffer
*/
#pragma once
#include <span>
#include <cstdint>
#include <cstring>

class VectorWriter {
    sVec<u8>& vector_;
public:
    VectorWriter(sVec<u8>& vector) : vector_(vector) {}

    template <typename T>
    void write(const T& val) const
    {
        size_t oldSize = vector_.size();
        vector_.resize(oldSize + sizeof(T));
        std::memcpy(vector_.data() + oldSize, &val, sizeof(T));
    }

    template <typename T>
    void writeBytes(const T* src, size_t size) const
    {
        size_t oldSize = vector_.size();
        vector_.resize(oldSize + size);
        std::memcpy(vector_.data() + oldSize, src, size);
    }

    size_t size() const { return vector_.size(); }
};

class BufferWriter {
    uint8_t* data_;
    size_t capacity_;
    size_t offset_ = 0;
public:
    BufferWriter(uint8_t* data, size_t capacity) : data_(data), capacity_(capacity) {}

    template <typename T>
    bool write(const T& val) {
        if (offset_ + sizeof(T) > capacity_) return false;
        std::memcpy(data_ + offset_, &val, sizeof(T));
        offset_ += sizeof(T);
        return true;
    }

    bool writeBytes(const void* src, size_t size) {
        if (offset_ + size > capacity_) return false;
        std::memcpy(data_ + offset_, src, size);
        offset_ += size;
        return true;
    }

    size_t size() const { return offset_; }
};

class BufferReader {
    const uint8_t* data_;
    size_t size_;
    size_t offset_ = 0;
public:
    BufferReader(const uint8_t* data, size_t size, size_t offset = 0) : data_(data), size_(size), offset_(offset) {}
    BufferReader(std::span<const uint8_t> span, size_t offset = 0) : data_(span.data()), size_(span.size()), offset_(offset)  {}

    // Reads a primitive or POD type safely with bounds checking
    template <typename T>
    bool read(T& val) {
        if (offset_ + sizeof(T) > size_) return false;
        std::memcpy(&val, data_ + offset_, sizeof(T));
        offset_ += sizeof(T);
        return true;
    }

    // Reads raw bytes into a destination buffer
    bool readBytes(void* dest, size_t size) {
        if (offset_ + size > size_) return false;
        std::memcpy(dest, data_ + offset_, size);
        offset_ += size;
        return true;
    }

    size_t remaining() const { return size_ - offset_; }
    size_t offset() const { return offset_; }
};