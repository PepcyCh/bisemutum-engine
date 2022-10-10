#pragma once

#include <functional>
#include <filesystem>

#include "bismuth.hpp"

BISMUTH_NAMESPACE_BEGIN

inline size_t HashCombine(size_t seed, size_t v) {
    seed ^= v + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
}
template <typename T>
size_t HashCombine(size_t seed, const T &v) {
    std::hash<T> hasher;
    return HashCombine(seed, hasher(v));
}

template <typename T>
size_t Hash(const T &v) {
    std::hash<T> hasher;
    return hasher(v);
}

template <typename T, typename... Args>
size_t Hash(const T &first, Args &&... last) {
    size_t result = Hash(last...);
    return HashCombine(result, first);
}


[[noreturn]] inline void Unreachable() {
    abort();
}


std::filesystem::path CurrentExecutablePath();

BISMUTH_NAMESPACE_END
