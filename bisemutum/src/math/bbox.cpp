#include <bisemutum/math/bbox.hpp>

namespace bi {

const BoundingBox BoundingBox::empty = BoundingBox{};

auto BoundingBox::reset() -> void {
    p_min = float3(std::numeric_limits<float>::max());
    p_max = float3(std::numeric_limits<float>::lowest());
}

auto BoundingBox::surf_area() const -> float {
    auto diff = math::max(p_max - p_min, 0.0f);
    return 2.0f * (diff.x * diff.y + diff.y * diff.z + diff.z * diff.x);
}

auto BoundingBox::union_with(BoundingBox const& rhs) const -> BoundingBox {
    return BoundingBox{
        .p_min = math::min(p_min, rhs.p_min),
        .p_max = math::max(p_max, rhs.p_max),
    };
}

auto BoundingBox::intersect_with(BoundingBox const& rhs) const -> BoundingBox {
    return BoundingBox{
        .p_min = math::max(p_min, rhs.p_min),
        .p_max = math::min(p_max, rhs.p_max),
    };
}

auto BoundingBox::add(float3 const& point) -> BoundingBox& {
    p_min = math::min(p_min, point);
    p_max = math::max(p_max, point);
    return *this;
}

auto BoundingBox::add(BoundingBox const& bbox) -> BoundingBox& {
    p_min = math::min(p_min, bbox.p_min);
    p_max = math::max(p_max, bbox.p_max);
    return *this;
}

}
