#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "../prelude/hash.hpp"

namespace bi {

template <typename V>
using StringHashMap = std::unordered_map<std::string, V, StringHash, std::equal_to<>>;

template <typename K, typename V> requires traits::Hashable<K, ByteHash<K>>
using ByteHashMap = std::unordered_map<K, V, ByteHash<K>>;

using StringHashSet = std::unordered_set<std::string, StringHash, std::equal_to<>>;

template <typename T> requires traits::Hashable<T, ByteHash<T>>
using ByteHashSet = std::unordered_set<T, ByteHash<T>>;

}

template<typename T> requires bi::traits::Hashable<T, std::hash<T>>
struct std::hash<std::vector<T>> final {
    auto operator()(std::vector<T> const& v) const noexcept -> size_t {
        size_t hash = 0;
        for (auto const& elem : v) {
            hash = bi::hash_combine(hash, elem);
        }
        return hash;
    }
};
