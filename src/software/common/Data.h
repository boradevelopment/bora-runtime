#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>

struct ShaderData {
    bool valid = false;
    sVec<u8> data;
    sVec<u8> ogData;
};

class Data {
public:
    // Constructor from file path
    Data(sString path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            state = -1;
            throw std::runtime_error("Failed to open file: " + path);
            return;
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        data.resize(static_cast<size_t>(size));
        if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
            state = -2;
            throw std::runtime_error("Failed to read file: " + path);
        }

        file.close();

        state = 1; // success
    }

    // Constructor from vector [copy]
    Data(const sVec<u8>& input) : data(input), state(1) {}

    // Constructor from vector [move]
    Data(sVec<u8>&& input) : data(std::move(input)), state(1) {}

    // Constructor from raw pointer and size [copy]
    Data(const u8* input, const size_t size) {
        if (!input || size == 0) {
            state = -3;
            throw std::runtime_error("Invalid pointer or size");
        }

        data.assign(input, input + size);
        state = 1;
    }

    // Constructor from raw pointer and size [move]
    Data(u8* input, size_t size) {
        if (!input || size == 0) {
            state = -3;
            throw std::runtime_error("Invalid pointer or size");
        }

        data.assign(input, input + size);
        state = 1;

        delete[] input;
    }

    void Release() {
        data.clear();
        delete this;
    }

public:
    sVec<u8> getData() {
        return data;
    }

    sVec<u8>* getDataPtr() {
        return &data;
    }

    int getState() {
        return state;
    }

    sVec<u8> operator()() {
        return data;
    }


private:
    sVec<u8> data;
    int state = 0;
};