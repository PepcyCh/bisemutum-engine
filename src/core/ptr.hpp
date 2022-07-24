#pragma once

#include <memory>

#include "bismuth.hpp"

BISMUTH_NAMESPACE_BEGIN

template <typename T, typename D = std::default_delete<T>>
using Ptr = std::unique_ptr<T, D>;



template <typename T>
using Ref = T *;

BISMUTH_NAMESPACE_END
