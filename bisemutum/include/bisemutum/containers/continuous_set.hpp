#pragma once

#include <vector>
#include <unordered_map>
#include <algorithm>

namespace bi {
    template <typename T, typename Hash = std::hash<T>>
    struct ContinuousSet final {
        static constexpr auto invalid_index = static_cast<size_t>(-1);

        auto size() const -> size_t { return elems.size(); }
        auto empty() const -> bool { return elems.empty(); }

        auto index_of(T const& elem) const -> size_t {
            if (auto it = index_map.find(elem); it != index_map.end()) {
                return it->second;
            }
            return invalid_index;
        }

        auto insert(T const& elem) -> size_t {
            auto [it, need_to_insert] = index_map.try_emplace(elem);
            if (need_to_insert) {
                it->second = elems.size();
                elems.push_back(elem);
                version_dirty = true;
            }
            return it->second;
        }
        auto insert(T&& elem) -> size_t {
            auto [it, need_to_insert] = index_map.try_emplace(elem);
            if (need_to_insert) {
                it->second = elems.size();
                elems.push_back(std::move(elem));
                version_dirty = true;
            }
            return it->second;
        }

        auto erase(T const &elem) -> void {
            if (auto it = index_map.find(elem); it != index_map.end()) {
                version_dirty = true;
                if (it->second != elems.size() - 1) {
                    std::swap(elems[it->second], elems.back());
                    index_map[elems[it->second]] = it->second;
                }
                elems.pop_back();
                index_map.erase(it);
            }
        }

        template <typename Container> requires requires (Container c) { c.begin(); c.end(); }
        auto erase(Container const& elems_to_be_erased) -> void {
            std::vector<size_t> erased_index;
            erased_index.reserve(elems_to_be_erased.size());

            for (auto& elem: elems_to_be_erased) {
                if (auto it = index_map.find(elem); it != index_map.end()) {
                    erased_index.push_back(it->second);
                    index_map.erase(it);
                }
            }
            version_dirty |= !erased_index.empty();

            std::sort(erased_index.begin(), erased_index.end());
            auto last_index = elems.size() - 1;
            for (auto i = erased_index.size() - 1; i < erased_index.size(); i--, last_index--) {
                auto index = erased_index[i];
                if (index != last_index) {
                    std::swap(elems[index], elems[last_index]);
                    index_map[elems[index]] = index;
                }
            }

            elems.resize(elems.size() - erased_index.size());
        }

        auto get_version() -> uint64_t {
            if (version_dirty) {
                ++version;
                version_dirty = false;
            }
            return version;
        }

        auto begin() -> std::vector<T>::iterator { return elems.begin(); }
        auto end() -> std::vector<T>::iterator { return elems.end(); }
        auto begin() const -> std::vector<T>::const_iterator { return elems.begin(); }
        auto end() const -> std::vector<T>::const_iterator { return elems.end(); }

    private:
        std::vector<T> elems;
        std::unordered_map<T, size_t, Hash> index_map;

        uint64_t version{0};
        bool version_dirty{false};
    };
}
