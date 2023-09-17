#pragma once

#include "../math/math_serde.hpp"
#include "../graphics/camera.hpp"

namespace bi {

enum class CameraRenderTargetSizeMode : uint8_t {
    fixed,
    window,
};

struct CameraComponent final {
    static constexpr std::string_view component_type_name = "CameraComponent";

    float yfov = 30.0f;
    float near_z = 0.001f;
    float far_z = 100000.0f;
    gfx::ProjectionType projection_type = gfx::ProjectionType::perspective;

    uint2 render_target_size = {1, 1};
    CameraRenderTargetSizeMode render_target_size_mode = CameraRenderTargetSizeMode::window;
    rhi::ResourceFormat render_target_format = rhi::ResourceFormat::rgba8_unorm;
};
BI_SREFL(
    type(CameraComponent),
    field(yfov),
    field(near_z),
    field(far_z),
    field(projection_type),
    field(render_target_size),
    field(render_target_format)
)

}
