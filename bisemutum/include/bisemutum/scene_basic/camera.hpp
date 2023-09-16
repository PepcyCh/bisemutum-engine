#pragma once

#include "../math/math_serde.hpp"
#include "../graphics/camera.hpp"

namespace bi {

struct CameraComponent final {
    static constexpr std::string_view component_type_name = "CameraComponent";

    float3 position = {0.0f, 0.0f, -1.0f};
    float3 front_dir = {0.0f, 0.0f, 1.0f};
    float3 up_dir = {0.0f, 1.0f, 0.0f};
    float yfov = 30.0f;
    float near_z = 0.001f;
    float far_z = 100000.0f;
    gfx::ProjectionType projection_type = gfx::ProjectionType::perspective;

    uint2 render_target_size = {1, 1};
    rhi::ResourceFormat render_target_format = rhi::ResourceFormat::rgba8_unorm;
};
BI_SREFL(
    type(CameraComponent),
    field(position),
    field(front_dir),
    field(up_dir),
    field(yfov),
    field(near_z),
    field(far_z),
    field(projection_type),
    field(render_target_size),
    field(render_target_format)
)

}
