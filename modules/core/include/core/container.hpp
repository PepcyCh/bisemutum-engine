#pragma once

#include <array>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>

#include "utils.hpp"
#include "traits.hpp"

BISMUTH_NAMESPACE_BEGIN

template <typename T>
using Vec = std::vector<T>;

template <typename T, size_t N>
using Array = std::array<T, N>;

template <typename T>
struct ByteHash {
    size_t operator()(const T &v) const noexcept {
        const uint8_t *p = reinterpret_cast<const uint8_t *>(&v);
        const size_t size = sizeof(v);
        size_t hash = 0;
        for (size_t i = 0; i < size; i++) {
            hash = HashCombine(hash, p[i]);
        }
        return hash;
    }
};

template <typename K, typename V>
using TreeMap = std::map<K, V>;

template <typename K, typename V, typename H = std::hash<K>> requires Hashable<K, H>
using HashMap = std::unordered_map<K, V, H>;

template <typename K, typename V> requires Hashable<K, ByteHash<K>>
using ByteHashMap = HashMap<K, V, ByteHash<K>>;

template <typename T>
using TreeSet = std::set<T>;

template <typename T, typename H = std::hash<T>> requires Hashable<T, H>
using HashSet = std::unordered_set<T, H>;

template <typename T> requires Hashable<T, ByteHash<T>>
using ByteHashSet = HashSet<T, ByteHash<T>>;

BISMUTH_NAMESPACE_END

template<typename T1, typename T2> requires bismuth::Hashable<T1, std::hash<T1>> && bismuth::Hashable<T2, std::hash<T2>>
struct std::hash<std::pair<T1, T2>> {
    size_t operator()(const std::pair<T1, T2> &v) const noexcept {
        return bismuth::Hash(v.first, v.second);
    }
};

template<typename T> requires bismuth::Hashable<T, std::hash<T>>
struct std::hash<std::vector<T>> {
    size_t operator()(const std::vector<T> &v) const noexcept {
        size_t hash = 0;
        for (const auto &elem : v) {
            hash = bismuth::HashCombine(hash, elem);
        }
        return hash;
    }
};
