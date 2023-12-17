#pragma once

#include <vector>
#include <compare>

namespace bi {

struct vector_bool;

template <bool is_const>
struct vector_bool_iterator_base final {
    using base_t = std::conditional_t<is_const, std::vector<uint8_t>::const_iterator, std::vector<uint8_t>::iterator>;
    using iterator_category = std::contiguous_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = bool;
    using pointer = std::conditional_t<is_const, const value_type *, value_type *>;
    using reference = std::conditional_t<is_const, const value_type &, value_type &>;

    vector_bool_iterator_base() = default;

    auto operator*() const -> reference { return reinterpret_cast<reference>(it_.operator*()); }
    auto operator->() const -> pointer { return reinterpret_cast<pointer>(it_.operator->()); }

    auto operator++() -> vector_bool_iterator_base& {
        ++it_;
        return *this;
    }
    auto operator++(int) -> vector_bool_iterator_base {
        auto ret = *this;
        ++(*this);
        return ret;
    }

    auto operator--() -> vector_bool_iterator_base& {
        --it_;
        return *this;
    }
    auto operator--(int) -> vector_bool_iterator_base {
        auto ret = *this;
        --(*this);
        return ret;
    }

    auto operator+(difference_type offset) const -> vector_bool_iterator_base {
        return vector_bool_iterator_base{it_ + offset};
    }
    auto operator+=(difference_type offset) -> vector_bool_iterator_base& {
        it_ += offset;
        return *this;
    }
    friend auto operator+(difference_type offset, vector_bool_iterator_base it) -> vector_bool_iterator_base {
        return it + offset;
    }

    auto operator-(difference_type offset) const -> vector_bool_iterator_base {
        return vector_bool_iterator_base{it_ - offset};
    }
    auto operator-=(difference_type offset) -> vector_bool_iterator_base& {
        it_ -= offset;
        return *this;
    }
    friend auto operator-(difference_type offset, vector_bool_iterator_base it) -> vector_bool_iterator_base {
        return it - offset;
    }

    auto operator[](difference_type offset) const -> reference {
        return reinterpret_cast<reference>(it_[offset]);
    }

    auto operator-(vector_bool_iterator_base rhs) const -> difference_type {
        return it_ - rhs.it_;
    }

    auto operator==(const vector_bool_iterator_base &rhs) const -> bool = default;
    auto operator<=>(const vector_bool_iterator_base &rhs) const -> std::strong_ordering = default;

private:
    friend vector_bool;
    vector_bool_iterator_base(base_t it) : it_(it) {}
    base_t it_;
};
using vector_bool_iterator = vector_bool_iterator_base<false>;
using vector_bool_const_iterator = vector_bool_iterator_base<true>;

// Due to specialization of `vector<bool>`, make a real vector<bool> here
struct vector_bool final {
    using base_t = std::vector<uint8_t>;
    using value_type = bool;
    using allocator_type = base_t::allocator_type;
    using size_type = base_t::size_type;
    using difference_type = base_t::difference_type;
    using reference = value_type &;
    using const_reference = const value_type &;
    using pointer = value_type *;
    using const_pointer = const value_type *;
    using iterator = vector_bool_iterator;
    using const_iterator = vector_bool_const_iterator;

    vector_bool() = default;
    vector_bool(size_t count, bool value = false) : base_(count, value) {}
    vector_bool(std::initializer_list<bool> list) : base_(list.size()) {
        std::memcpy(base_.data(), list.begin(), base_.size() * sizeof(bool));
    }

    vector_bool(vector_bool const& rhs) : base_(rhs.base_) {}
    vector_bool(vector_bool&& rhs) : base_(std::move(rhs.base_)) {}
    auto operator=(vector_bool const& rhs) -> vector_bool& {
        base_ = rhs.base_;
        return *this;
    }
    auto operator=(vector_bool&& rhs) -> vector_bool& {
        base_ = std::move(rhs.base_);
        return *this;
    }

    auto operator[](size_t index) -> reference { return reinterpret_cast<reference>(base_[index]); }
    auto operator[](size_t index) const -> value_type { return base_[index]; }
    auto at(size_t index) -> reference { return reinterpret_cast<reference>(base_.at(index)); }
    auto at(size_t index) const -> value_type { return base_.at(index); }

    auto begin() -> iterator { return {base_.begin()}; }
    auto end() -> iterator { return {base_.end()}; }
    auto cbegin() const -> const_iterator { return {base_.begin()}; }
    auto cend() const -> const_iterator  { return {base_.end()}; }
    auto begin() const -> const_iterator { return {base_.begin()}; }
    auto end() const -> const_iterator { return {base_.end()}; }

    auto data() -> pointer { return reinterpret_cast<pointer>(base_.data()); }
    auto data() const -> const_pointer { return reinterpret_cast<const_pointer>(base_.data()); }

    auto front() -> reference { return reinterpret_cast<reference>(base_.front()); }
    auto front() const -> value_type { return base_.front(); }
    auto back() -> reference { return reinterpret_cast<reference>(base_.back()); }
    auto back() const -> value_type { return base_.back(); }

    auto assign(size_t count, bool value) -> void {
        base_.assign(count, value);
    }
    auto assign(std::initializer_list<bool> list) -> void {
        base_.resize(list.size());
        std::memcpy(base_.data(), list.begin(), base_.size() * sizeof(bool));
    }

    auto size() const -> size_t { return base_.size(); }
    auto empty() const -> bool { return base_.empty(); }
    auto max_size() const -> size_t { return base_.max_size(); }
    auto capacity() const -> size_t { return base_.capacity(); }

    auto resize(size_t count, bool value = false) -> void { base_.resize(count, value); }
    auto reserve(size_t count) -> void { base_.reserve(count); }
    auto shrink_to_fit() -> void { base_.shrink_to_fit(); }

    auto clear() -> void { base_.clear(); }
    auto push_back(bool value) -> void { base_.push_back(value); }
    auto emplack_back(bool value = false) -> reference { return reinterpret_cast<reference>(base_.emplace_back(value)); }
    auto pop_back() -> void { base_.pop_back(); }

    auto insert(const_iterator pos, bool value) -> iterator { return {base_.insert(pos.it_, value)}; }
    auto insert(const_iterator pos, size_t count, bool value) -> iterator { return {base_.insert(pos.it_, count, value)}; }
    template <typename InputIt>
    auto insert(const_iterator pos, InputIt first, InputIt last) -> iterator { return {base_.insert(pos.it_, first.it_, last.it_)}; }

    auto erase(iterator pos) -> iterator { return {base_.erase(pos.it_)}; }
    auto erase(const_iterator pos) -> iterator { return {base_.erase(pos.it_)}; }
    auto erase(iterator first, iterator last) -> iterator { return {base_.erase(first.it_, last.it_)}; }
    auto erase(const_iterator first, const_iterator last) -> iterator { return {base_.erase(first.it_, last.it_)}; }

    auto swap(vector_bool &rhs) -> void { base_.swap(rhs.base_); }

    auto operator==(vector_bool const& rhs) const -> bool = default;
    auto operator<=>(vector_bool const& rhs) const -> std::weak_ordering = default;

private:
    std::vector<uint8_t> base_;
};

}
