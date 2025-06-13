// Apart of the BORA Runtime Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.

/* This header file is a common file but the source file is PD (Platform Dependent)

 * FileName: atomic.h
 * Title: Atomic
 * Author: WanoManiac
 * Purpose: Allows WASM access to atomic variables towards

 * Compatibility: Tested on C++ 17 | C++ 11

 * Updates - ?
 * Known issues - ?
 */

#ifndef BORA_ATOMIC_H
#define BORA_ATOMIC_H

#include <mutex>
#include "hostSymbolTemplate.hpp"

class boraAtomicSymbols : public HostSymbolTemplate {
public:
    const char *get_namespace() const override {
        return "bora::stl::atomic";
    }

    static std::unordered_map<uint64_t, std::atomic<uint64_t>> atomic_map;
    static std::mutex atomic_map_mutex;

    static uint64_t atomic_create(uint64_t initial_value);

// Load from atomic
    static uint64_t atomic_load(uint64_t atomic_id);

   // Store
    static void atomic_store(uint64_t atomic_id, uint64_t value);

// Compare and exchange
    static bool atomic_compare_exchange(uint64_t atomic_id, uint64_t expected, uint64_t desired);


    std::vector<NativeSymbol> symbols = {
            { "create", (void*)atomic_create, "(l)l", nullptr },
            { "load", (void*)atomic_load, "(l)l", nullptr },
            { "store", (void*)atomic_store, "(ll)", nullptr },
            { "compare_exchange", (void*)atomic_compare_exchange, "(lll)i", nullptr },
    };

    const std::vector<NativeSymbol>& get_symbols() const override {
        return symbols;
    }
};

#endif //BORA_ATOMIC_H
