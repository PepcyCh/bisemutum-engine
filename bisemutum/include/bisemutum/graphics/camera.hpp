#pragma once

#include "resource.hpp"
#include "shader_param.hpp"
#include "../math/math.hpp"

namespace bi::gfx {

enum class ProjectionType : uint8_t {
    perspective,
    orthographic,
};

struct GraphicsManager;

struct Camera final {
    auto recreate_target_texture(
        uint32_t width, uint32_t height, rhi::ResourceFormat format, bool mipmap = true
    ) -> void;
    auto recreate_target_texture(rhi::TextureDesc const& desc) -> void;

    auto target_texture() -> Texture& { return target_texture_; }
    auto target_texture() const -> Texture const& { return target_texture_; }

    auto shader_params() -> ShaderParameter&;
    auto shader_params_metadata() const -> ShaderParameterMetadataList const&;
    auto update_shader_params() -> void;

    // The value is updated after `update_shader_params()`.
    auto matrix_proj_view() const -> float4x4 const& { return matrix_proj_view_; }

    float3 position = float3{0.0f, 0.0f, -1.0f};
    float3 front_dir = float3{0.0f, 0.0f, 1.0f};
    float3 up_dir = float3{0.0f, 1.0f, 0.0f};
    float yfov = 30.0f;
    float near_z = 0.001f;
    float far_z = 100000.0f;
    ProjectionType projection_type = ProjectionType::perspective;

    bool enabled = true;

private:
    friend GraphicsManager;

    Texture target_texture_;

    ShaderParameter shader_parameter_;
    float4x4 matrix_proj_view_ = float4x4(1.0f);
};

}
