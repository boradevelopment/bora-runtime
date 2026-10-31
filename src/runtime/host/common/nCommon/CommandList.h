// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: CommandList.h
 * Purpose: Command lists are the universal way of communicating between ends without
 */

#pragma once
#include "nCommon/CommandListContainers.h"

template<CommandListContainer T>
class CommandList {
public:
    explicit CommandList(T* vector) : commands(vector) {}
    T* commands;
    explicit operator T* () const { return commands; }
    T* getCommands() const { return commands; }
};
