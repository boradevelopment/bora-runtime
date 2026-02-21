// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: CommandList.h
 * Purpose: Command lists are the universal way of communicating between ends without
 */

#pragma once
#include <optional>

template<typename C>
concept CommandListContainer =
    requires(C c, typename C::value_type v) {
    // required type
    typename C::value_type;

    // mutation
    { c.push_back(v) } -> std::same_as<void>;
    { c.push_back(std::move(v)) } -> std::same_as<void>;

    // removal
    { c.pop_back() } -> std::same_as<std::optional<typename C::value_type>>;

    // size
    { c.size() } -> std::convertible_to<std::size_t>;
    };

template<CommandListContainer T>
class CommandList {
public:
    explicit CommandList(T* vector) : commands(vector) {}
    T* commands;
    explicit operator T* () const { return commands; }
    T* getCommands() const { return commands; }
};