#pragma once

#include <limits>

#include "math.hpp"

namespace bi {

struct BoundingBox final {
    auto center() const -> float3 { return (p_min + p_max) * 0.5f; }
    auto extent() const -> float3 { return p_max - p_min; }
    auto is_empty() const -> bool { return math::any(math::greaterThanEqual(p_min, p_max)); }
    auto reset() -> void;

    auto surf_area() const -> float;

    auto union_with(BoundingBox const& rhs) const -> BoundingBox;
    auto intersect_with(BoundingBox const& rhs) const -> BoundingBox;

    auto add(float3 const& point) -> BoundingBox&;
    auto add(BoundingBox const& bbox) -> BoundingBox&;

    static const BoundingBox empty;

    float3 p_min = float3(std::numeric_limits<float>::max());
    float3 p_max = float3(std::numeric_limits<float>::lowest());
};

}
