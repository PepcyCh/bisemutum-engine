#include <bisemutum/math/transform.hpp>

#include <glm/gtx/transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <bisemutum/runtime/scene_object.hpp>
#include <bisemutum/editor/basic.hpp>

namespace bi {

auto Transform::matrix() const -> float4x4 {
    return float4x4(
        float4(rotation[0] * scaling.x, 0.0f),
        float4(rotation[1] * scaling.y, 0.0f),
        float4(rotation[2] * scaling.z, 0.0f),
        float4(translation, 1.0f)
    );
}

auto Transform::matrix_transposed_inverse() const -> float4x4 {
    return float4x4(
        float4(rotation[0], -math::dot(rotation[0], translation)) / scaling.x,
        float4(rotation[1], -math::dot(rotation[1], translation)) / scaling.y,
        float4(rotation[2], -math::dot(rotation[2], translation)) / scaling.z,
        float4(0.0f, 0.0f, 0.0f, 1.0f)
    );
}

auto Transform::from_matrix(float4x4 const& matrix) -> Transform {
    Transform trans{};
    trans.translation = matrix[3];
    trans.rotation = matrix;
    trans.scaling.x = glm::length(trans.rotation[0]);
    if (trans.scaling.x != 0.0f) { trans.rotation[0] /= trans.scaling.x; }
    trans.scaling.y = glm::length(trans.rotation[1]);
    if (trans.scaling.y != 0.0f) { trans.rotation[1] /= trans.scaling.y; }
    trans.scaling.z = glm::length(trans.rotation[2]);
    if (trans.scaling.z != 0.0f) { trans.rotation[2] /= trans.scaling.z; }
    return trans;
}

auto Transform::transform_position(float3 const& pos) const -> float3 {
    return rotation * (scaling * pos) + translation;
}

auto Transform::transform_direction(float3 const& dir) const -> float3 {
    return rotation * (scaling * dir);
}

auto Transform::transform_bounding_box(BoundingBox const& bbox) const -> BoundingBox {
    auto p0 = transform_position(float3(bbox.p_min.x, bbox.p_min.y, bbox.p_min.z));
    auto p1 = transform_position(float3(bbox.p_min.x, bbox.p_min.y, bbox.p_max.z));
    auto p2 = transform_position(float3(bbox.p_min.x, bbox.p_max.y, bbox.p_min.z));
    auto p3 = transform_position(float3(bbox.p_min.x, bbox.p_max.y, bbox.p_max.z));
    auto p4 = transform_position(float3(bbox.p_max.x, bbox.p_min.y, bbox.p_min.z));
    auto p5 = transform_position(float3(bbox.p_max.x, bbox.p_min.y, bbox.p_max.z));
    auto p6 = transform_position(float3(bbox.p_max.x, bbox.p_max.y, bbox.p_min.z));
    auto p7 = transform_position(float3(bbox.p_max.x, bbox.p_max.y, bbox.p_max.z));
    return BoundingBox{
        .p_min = math::min(
            math::min(math::min(p0, p1), math::min(p2, p3)),
            math::min(math::min(p4, p5), math::min(p6, p7))
        ),
        .p_max = math::max(
            math::max(math::max(p0, p1), math::max(p2, p3)),
            math::max(math::max(p4, p5), math::max(p6, p7))
        ),
    };
}

auto Transform::operator*(Transform const& rhs) const -> Transform {
    return from_matrix(matrix() * rhs.matrix());
}

auto Transform::inverse() const -> Transform {
    return from_matrix(math::transpose(matrix_transposed_inverse()));
}

auto Transform::edit() -> bool {
    float3 rotation_angles{};
    math::extractEulerAngleZXY(float4x4{rotation}, rotation_angles.z, rotation_angles.x, rotation_angles.y);
    rotation_angles = math::degrees(rotation_angles);
    auto dirty = editor::edit_float3("translation", translation);
    dirty |= editor::edit_float3("scaling", scaling);
    auto rotation_dirty = editor::edit_float3("rotation", rotation_angles);
    if (rotation_dirty) {
        dirty = true;
        rotation_angles = math::radians(rotation_angles);
        rotation = math::eulerAngleZXY(rotation_angles.z, rotation_angles.x, rotation_angles.y);
    }
    return dirty;
}

auto Transform::to_value(serde::Value &v, Transform const& o) -> void {
    serde::to_value(v["translation"], o.translation);
    serde::to_value(v["scaling"], o.scaling);
    float3 rotation{};
    math::extractEulerAngleZXY(float4x4{o.rotation}, rotation.z, rotation.x, rotation.y);
    serde::to_value(v["rotation"], math::degrees(rotation));
}

auto Transform::from_value(serde::Value const& v, Transform& o) -> void {
    o.translation = v.value("translation", float3{0.0f});
    o.scaling = v.value("scaling", float3{1.0f});
    auto rotation = math::radians(v.value("rotation", float3{0.0f}));
    o.rotation = math::eulerAngleZXY(rotation.z, rotation.x, rotation.y);
}

}
