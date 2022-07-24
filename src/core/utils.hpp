#pragma once

#include <functional>

#include "bismuth.hpp"

BISMUTH_NAMESPACE_BEGIN

template <typename T>
size_t HashCombine(size_t seed, const T &v) {
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
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

BISMUTH_NAMESPACE_END
