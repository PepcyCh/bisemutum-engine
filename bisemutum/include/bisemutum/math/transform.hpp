#pragma once

#include "math.hpp"
#include "bbox.hpp"
#include "../utils/serde.hpp"

namespace bi {

namespace rt {

struct SceneObject;

}

struct Transform final {
    static constexpr std::string_view component_type_name = "Transform";

    float3 translation = float3{0.0f};
    float3x3 rotation = float3x3{1.0f};
    float3 scaling = float3{1.0f};

    auto set_rotation_with_quaternion(float4 quat) -> void;

    auto matrix() const -> float4x4;
    auto matrix_transposed_inverse() const -> float4x4;
    static auto from_matrix(float4x4 const& matrix) -> Transform;

    auto transform_position(float3 const& pos) const -> float3;
    auto transform_direction(float3 const& dir) const -> float3;
    auto transform_bounding_box(BoundingBox const& bbox) const -> BoundingBox;

    auto transform_position_without_scaling(float3 const& pos) const -> float3;
    auto transform_direction_without_scaling(float3 const& dir) const -> float3;

    auto operator*(Transform const& rhs) const -> Transform;

    auto inverse() const -> Transform;

    static auto to_value(serde::Value &v, Transform const& o) -> void;
    static auto from_value(serde::Value const& v, Transform& o) -> void;

    auto edit() -> bool;
};

BI_SREFL(
    type(Transform),
    field(translation),
    field(rotation),
    field(scaling)
)

}
