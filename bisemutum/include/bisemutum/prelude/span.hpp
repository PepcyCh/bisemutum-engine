#pragma once

#include <iterator>

#include "hash.hpp"
#include "traits.hpp"

namespace bi {

template <typename T>
struct Span final {
    Span() : data_(nullptr), size_(0) {}

    Span(T* data, size_t size) : data_(data), size_(size) {}

    Span(T* begin, T* end) : data_(begin), size_(end - begin) {}

    Span(std::initializer_list<T> list) : data_(list.begin()), size_(list.size()) {}

    template <traits::Range R>
    Span(R const& range) : data_(std::to_address(range.begin())), size_(std::distance(range.begin(), range.end())) {}

    auto size() const -> size_t { return size_; }
    auto data() const -> T* { return data_; }
    auto empty() const -> bool { return size_ == 0; }

    auto operator[](size_t index) const -> T& { return data_[index]; }

    auto begin() const -> T* { return data_; }
    auto end() const -> T* { return data_ + size_; }
    auto cbegin() const -> T const* { return data_; }
    auto cend() const -> T const* { return data_ + size_; }

    auto operator==(Span const& rhs) const -> bool {
        if (size_ != rhs.size_) {
            return false;
        }
        for (size_t i = 0; i < size_; i++) {
            if (data_[i] != rhs.data_[i]) {
                return false;
            }
        }
        return true;
    }

private:
    T* data_;
    size_t size_;
};

template <typename T>
using CSpan = Span<std::add_const_t<T>>;

}

template<typename T> requires bi::traits::Hashable<T, std::hash<T>>
struct std::hash<bi::Span<T>> final {
    auto operator()(bi::Span<T> const& v) const noexcept -> size_t {
        size_t hash = 0;
        for (auto const& elem : v) {
            hash = bi::hash_combine(hash, elem);
        }
        return hash;
    }
};
