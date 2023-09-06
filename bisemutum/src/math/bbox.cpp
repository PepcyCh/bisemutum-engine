#include <bisemutum/math/bbox.hpp>

namespace bi {

auto BoundingBox::surf_area() const -> float {
    auto diff = p_max - p_min;
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

}
