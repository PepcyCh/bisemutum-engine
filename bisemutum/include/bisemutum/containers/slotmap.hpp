#pragma once

#include <vector>

#include "../prelude/option.hpp"

namespace bi {

template <typename T>
concept SlotMapHandle = traits::StaticCastConvertibleTo<T, size_t> && traits::StaticCastConvertibleFrom<T, size_t>;

template <typename T, SlotMapHandle Handle = size_t>
struct SlotMap final {
    template <bool is_const>
    struct Iterator final {
    public:
        using iterator_category = std::input_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = std::conditional_t<is_const, value_type const*, value_type*>;
        using reference = std::conditional_t<is_const, value_type const&, value_type&>;

        using SlotMapRef = std::conditional_t<is_const, SlotMap<T, Handle> const&, SlotMap<T, Handle>&>;

        auto operator*() -> reference {
            return slotmap_.data_[index_].value();
        }
        auto operator->() const -> pointer {
            return &slotmap_.data_[index_].value();
        }

        auto operator++() -> Iterator& {
            if (index_ == slotmap_.data_.size()) {
                return *this;
            }
            do {
                ++index_;
            } while (index_ < slotmap_.data_.size() && !slotmap_.data_[index_].has_value());
            return *this;
        }
        auto operator++(int) -> Iterator {
            auto ret = *this;
            ++(*this);
            return ret;
        }

        auto operator==(Iterator const& rhs) const -> bool {
            return index_ == rhs.index_ && &slotmap_ == &rhs.slotmap_;
        }

    private:
        friend SlotMap;

        Iterator(SlotMapRef slotmap, size_t index) : index_(index), slotmap_(slotmap) {}

        size_t index_ = 0;
        SlotMapRef slotmap_;
    };

    template <bool is_const>
    struct PairsHelper;
    template <bool is_const>
    struct PairIterator final {
    public:
        using SlotMapRef = std::conditional_t<is_const, SlotMap<T, Handle> const&, SlotMap<T, Handle>&>;

        auto operator*() -> decltype(auto) {
            return std::make_pair(static_cast<Handle>(index_), make_ref(slotmap_.data_[index_].value()));
        }

        auto operator++() -> PairIterator& {
            if (index_ == slotmap_.data_.size()) {
                return *this;
            }
            do {
                ++index_;
            } while (index_ < slotmap_.data_.size() && !slotmap_.data_[index_].has_value());
            return *this;
        }
        auto operator++(int) -> PairIterator {
            auto ret = *this;
            ++(*this);
            return ret;
        }

        auto operator==(PairIterator const& rhs) const -> bool {
            return index_ == rhs.index_ && &slotmap_ == &rhs.slotmap_;
        }

    private:
        friend SlotMap;
        template <bool is_const_>
        friend struct PairsHelper;

        PairIterator(SlotMapRef slotmap, size_t index) : index_(index), slotmap_(slotmap) {}

        size_t index_ = 0;
        SlotMapRef slotmap_;
    };

    template <bool is_const>
    struct PairsHelper final {
        using SlotMapRef = std::conditional_t<is_const, SlotMap<T, Handle> const&, SlotMap<T, Handle>&>;

        auto begin() -> PairIterator<is_const> { return PairIterator<is_const>{slotmap_, slotmap_.first_valid_index()}; }
        auto end() -> PairIterator<is_const> { return PairIterator<is_const>{slotmap_, slotmap_.data_.size()}; }

    private:
        friend SlotMap;

        PairsHelper(SlotMapRef slotmap) : slotmap_(slotmap) {}

        SlotMapRef slotmap_;
    };

    auto size() const -> size_t { return data_.size() - freed_indices_.size(); }
    auto capacity() const -> size_t { return data_.size(); }

    auto begin() -> Iterator<false> { return Iterator<false>{*this, first_valid_index()}; }
    auto end() -> Iterator<false> { return Iterator<false>{*this, data_.size()}; }
    auto cbegin() const -> Iterator<true> { return Iterator<true>{*this, first_valid_index()}; }
    auto cend() const -> Iterator<true> { return Iterator<true>{*this, data_.size()}; }
    auto begin() const -> Iterator<true> { return Iterator<true>{*this, first_valid_index()}; }
    auto end() const -> Iterator<true> { return Iterator<true>{*this, data_.size()}; }

    auto pairs() -> PairsHelper<false> { return PairsHelper<false>(*this); }
    auto const_pairs() const -> PairsHelper<true> { return PairsHelper<true>(*this); }
    auto pairs() const -> PairsHelper<true> { return PairsHelper<true>(*this); }

    auto insert(T const& value) -> Handle {
        if (!freed_indices_.empty()) {
            auto index = freed_indices_.back();
            freed_indices_.pop_back();
            data_[index] = value;
            return static_cast<Handle>(index);
        } else {
            data_.push_back(value);
            return static_cast<Handle>(data_.size() - 1);
        }
    }
    auto insert(T&& value) -> Handle {
        if (!freed_indices_.empty()) {
            auto index = freed_indices_.back();
            freed_indices_.pop_back();
            data_[index] = std::move(value);
            return static_cast<Handle>(index);
        } else {
            data_.push_back(std::move(value));
            return static_cast<Handle>(data_.size() - 1);
        }
    }

    template <typename... Args>
    auto emplace(Args&&... args) -> Handle {
        if (!freed_indices_.empty()) {
            auto index = freed_indices_.back();
            freed_indices_.pop_back();
            data_[index].emplace(std::forward<Args>(args)...);
            return static_cast<Handle>(index);
        } else {
            data_.push_back(Option<T>::make(std::forward<Args>(args)...));
            return static_cast<Handle>(data_.size() - 1);
        }
    }

    auto try_get(Handle handle) -> T* {
        if (auto& value = data_[static_cast<size_t>(handle)]; value.has_value()) {
            return &value.value();
        } else {
            return nullptr;
        }
    }
    auto try_get(Handle handle) const -> T const* {
        if (auto& value = data_[static_cast<size_t>(handle)]; value.has_value()) {
            return &value.value();
        } else {
            return nullptr;
        }
    }
    auto get(Handle handle) -> T& {
        return data_[static_cast<size_t>(handle)].value();
    }
    auto get(Handle handle) const -> T const& {
        return data_[static_cast<size_t>(handle)].value();
    }

    auto remove(Handle handle) -> void {
        auto index = static_cast<size_t>(handle);
        if (data_[index].has_value()) {
            freed_indices_.push_back(index);
            data_[index].reset();
        }
    }

private:
    auto first_valid_index() const -> size_t {
        size_t index = 0;
        while (index < data_.size() && !data_[index].has_value()) { ++index; }
        return index;
    }

    std::vector<Option<T>> data_;
    std::vector<size_t> freed_indices_;
};

}
