// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: ThreadBufferPool.h
 * Purpose: Maintains a scratch buffer per thread so we can reuse them
*/

#pragma once

class ThreadBufferPool {
public:
    // Retrieves a reference to a thread-local buffer, clears any old data,
    // (optional: minimum capacity to reduce allocations if needed.)
    static vec8& getScratchBuffer(size_t min_capacity = 256) {
        thread_local vec8 scratch_buffer;
        scratch_buffer.clear();

        if (scratch_buffer.capacity() < min_capacity) {
            scratch_buffer.reserve(min_capacity);
        }

        return scratch_buffer;
    }
};