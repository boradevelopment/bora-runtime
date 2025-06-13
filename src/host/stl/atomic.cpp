// Apart of the BORA Runtime Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.

#include "atomic.h"

std::unordered_map<uint64_t, std::atomic<uint64_t>> boraAtomicSymbols::atomic_map;
std::mutex boraAtomicSymbols::atomic_map_mutex;

uint64_t boraAtomicSymbols::atomic_create(uint64_t initial_value) {
    std::lock_guard<std::mutex> lock(atomic_map_mutex);
    static uint64_t id = 1;
    atomic_map[id] = initial_value;
    return id++;
}

uint64_t boraAtomicSymbols::atomic_load(uint64_t atomic_id) {
    std::lock_guard<std::mutex> lock(atomic_map_mutex);
    return atomic_map[atomic_id].load(std::memory_order_seq_cst);
}

void boraAtomicSymbols::atomic_store(uint64_t atomic_id, uint64_t value) {
    std::lock_guard<std::mutex> lock(atomic_map_mutex);
    atomic_map[atomic_id].store(value, std::memory_order_seq_cst);
}

bool boraAtomicSymbols::atomic_compare_exchange(uint64_t atomic_id, uint64_t expected, uint64_t desired) {
    std::lock_guard<std::mutex> lock(atomic_map_mutex);
    return atomic_map[atomic_id].compare_exchange_strong(expected, desired);
}
