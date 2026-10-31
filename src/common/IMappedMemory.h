// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: IMappedMemory.h
 * Purpose: ?
*/
#pragma once

class IMappedMemory {
public:
    virtual ~IMappedMemory() = default;

    virtual u8* getData() const = 0;
    virtual size_t getSize() const = 0;
    virtual int getFileDescriptor() const = 0;

    virtual void setOffset(u64 offset) = 0;

    virtual sUniquePointer<IMappedMemory> duplicate() = 0;
};