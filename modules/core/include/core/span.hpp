#pragma once

#include "bismuth.hpp"
#include "container.hpp"

BISMUTH_NAMESPACE_BEGIN

template <typename T>
class Span {
public:
    Span() : data_(nullptr), size_(0) {}

    Span(const T *data, size_t size) : data_(data), size_(size) {}

    Span(const T *begin, const T *end) : data_(begin), size_(end - begin) {}

    Span(std::initializer_list<T> list) : data_(list.begin()), size_(list.size()) {}

    template <size_t N>
    Span(const Array<T, N> &arr) : data_(arr.data()), size_(N) {}

    Span(const Vec<T> &vec) : data_(vec.data()), size_(vec.size()) {}

    const T *Data() const { return data_; }

    size_t Size() const { return size_; }

    bool Empty() const { return size_ == 0; }

    const T &operator[](size_t index) const { return data_[index]; }

    const T *begin() const { return data_; }
    const T *end() const { return data_ + size_; }
    const size_t size() const { return size_; }

private:
    const T *data_;
    size_t size_;
};

BISMUTH_NAMESPACE_END
