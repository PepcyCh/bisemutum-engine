#pragma once

#include "math.hpp"

namespace bi {

struct BoundingBox final {
    auto center() const -> float3 { return (p_min + p_max) * 0.5f; }
    auto extent() const -> float3 { return p_max - p_min; }
    auto is_empty() const -> bool { return math::any(math::greaterThanEqual(p_min, p_max)); }

    auto surf_area() const -> float;

    auto union_with(BoundingBox const& rhs) const -> BoundingBox;
    auto intersect_with(BoundingBox const& rhs) const -> BoundingBox;

    float3 p_min = float3(0.0f);
    float3 p_max = float3(0.0f);
};

}
