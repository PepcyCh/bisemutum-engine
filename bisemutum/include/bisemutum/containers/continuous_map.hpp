#pragma once

#include <vector>
#include <unordered_map>
#include <algorithm>

namespace bi {
    template <typename KeyT, typename ValueT, typename Hash = std::hash<KeyT>>
    struct ContinuousMap final {
        static constexpr auto invalid_index = static_cast<size_t>(-1);

        auto size() const -> size_t { return keys.size(); }
        auto empty() const -> bool { return keys.empty(); }

        auto index_of(KeyT const& key) const -> size_t {
            if (auto it = index_map.find(key); it != index_map.end()) {
                return it->second;
            }
            return invalid_index;
        }

        auto insert(KeyT const& key, ValueT const& value) -> size_t {
            auto [it, need_to_insert] = index_map.try_emplace(key);
            if (need_to_insert) {
                it->second = keys.size();
                keys.push_back(key);
                values.push_back(value);
                version_dirty = true;
            }
            return it->second;
        }
        auto insert(KeyT const& key, ValueT&& value) -> size_t {
            auto [it, need_to_insert] = index_map.try_emplace(key);
            if (need_to_insert) {
                it->second = keys.size();
                keys.push_back(key);
                values.emplace_back(std::move(value));
                version_dirty = true;
            }
            return it->second;
        }

        auto insert_or_assign(KeyT const& key, ValueT const& value) -> size_t {
            auto [it, need_to_insert] = index_map.try_emplace(key);
            if (need_to_insert) {
                it->second = keys.size();
                keys.push_back(key);
                values.push_back(value);
                version_dirty = true;
            } else {
                values[it->second] = value;
            }
            return it->second;
        }
        auto insert_or_assign(KeyT const& key, ValueT&& value) -> size_t {
            auto [it, need_to_insert] = index_map.try_emplace(key);
            if (need_to_insert) {
                it->second = keys.size();
                keys.push_back(key);
                values.emplace_back(std::move(value));
                version_dirty = true;
            } else {
                values[it->second] = std::move(value);
            }
            return it->second;
        }

        template <typename... Args> requires std::is_constructible_v<ValueT, Args...>
        auto try_emplace(KeyT const& key, Args&&... args) -> ValueT& {
            auto [it, need_to_insert] = index_map.try_emplace(key);
            if (need_to_insert) {
                it->second = keys.size();
                keys.push_back(key);
                values.emplace_back(forward<Args>(args)...);
                version_dirty = true;
            }
            return values[it->second];
        }
        template <typename... Args> requires std::is_constructible_v<ValueT, Args...>
        auto emplace(KeyT const& key, Args&&... args) -> ValueT& {
            auto [it, need_to_insert] = index_map.try_emplace(key);
            if (need_to_insert) {
                it->second = keys.size();
                keys.push_back(key);
                values.emplace_back(forward<Args>(args)...);
                version_dirty = true;
            } else {
                values[it->second] = ValueT{forward<Args>(args)...};
            }
            return values[it->second];
        }

        auto erase(KeyT const &key) -> void {
            if (auto it = index_map.find(key); it != index_map.end()) {
                version_dirty = true;
                if (it->second != keys.size() - 1) {
                    std::swap(keys[it->second], keys.back());
                    std::swap(values[it->second], values.back());
                    index_map[keys[it->second]] = it->second;
                }
                keys.pop_back();
                values.pop_back();
                index_map.erase(it);
            }
        }

        template <typename Container> requires requires (Container c) { c.begin(); c.end(); }
        auto erase(Container const& keys_to_be_erased) -> void {
            std::vector<size_t> erased_index;
            erased_index.reserve(keys_to_be_erased.size());

            for (auto& key: keys_to_be_erased) {
                if (auto it = index_map.find(key); it != index_map.end()) {
                    erased_index.push_back(it->second);
                    index_map.erase(it);
                }
            }
            version_dirty |= !erased_index.empty();

            std::sort(erased_index.begin(), erased_index.end());
            auto last_index = keys.size() - 1;
            for (auto i = erased_index.size() - 1; i < erased_index.size(); i--, last_index--) {
                auto index = erased_index[i];
                if (index != last_index) {
                    std::swap(keys[index], keys[last_index]);
                    std::swap(values[index], values[last_index]);
                    index_map[keys[index]] = index;
                }
            }

            keys.resize(keys.size() - erased_index.size());
            values.resize(values.size() - erased_index.size());
        }

        auto at(KeyT const& key) -> ValueT& {
            return values[index_map.at(key)];
        }
        auto at(KeyT const& key) const -> ValueT const& {
            return values[index_map.at(key)];
        }

        auto get_version() -> uint64_t {
            if (version_dirty) {
                ++version;
                version_dirty = false;
            }
            return version;
        }

        auto get_keys() const -> std::vector<KeyT> const& { return keys; }
        auto get_values() const -> std::vector<ValueT> const& { return values; }

    private:
        std::vector<KeyT> keys;
        std::vector<ValueT> values;
        std::unordered_map<KeyT, size_t, Hash> index_map;

        uint64_t version{0};
        bool version_dirty{false};
    };
}
