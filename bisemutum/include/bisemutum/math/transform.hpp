#include "math.hpp"
#include "bbox.hpp"

namespace bi {

struct Transform final {
    float3 translation = float3(0.0f);
    float3x3 rotation = float3x3(1.0f);
    float3 scaling = float3(1.0f);

    auto matrix() const -> float4x4;
    static auto from_matrix(float4x4 const& matrix) -> Transform;

    auto transform_position(float3 const& pos) const -> float3;
    auto transform_direction(float3 const& dir) const -> float3;
    auto transform_bounding_box(BoundingBox const& bbox) const -> BoundingBox;

    auto operator*(Transform const& rhs) const -> Transform;
};

}
