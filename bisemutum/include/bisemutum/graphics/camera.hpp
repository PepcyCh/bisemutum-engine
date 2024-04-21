#pragma once

#include "handles.hpp"
#include "resource.hpp"
#include "shader_param.hpp"
#include "../math/math.hpp"

namespace bi::gfx {

enum class ProjectionType : uint8_t {
    perspective,
    orthographic,
};

struct GraphicsManager;
struct RenderGraph;

// TODO - split camera data and camera used for rendering ?
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

    // The values are updated after `update_shader_params()`.
    auto matrix_proj_view() const -> float4x4 const& { return matrix_proj_view_; }
    auto history_matrix_proj_view() const -> float4x4 const& { return history_matrix_proj_view_; }

    // Plane is represented in the form of 'dot(p, n) + d = 0'.
    // Normal 'n' is xyz and distance 'd' is w.
    // All normals point to inner side of frustum.
    // Order is: front, back, top, down, left, right.
    auto get_frustum_planes() const -> std::array<float4, 6>;

    auto add_history_buffer(std::string key, BufferHandle handle) const -> void;
    auto add_history_texture(std::string key, TextureHandle handle) const -> void;
    auto get_history_buffer(std::string_view key) const -> BufferHandle;
    auto get_history_texture(std::string_view key) const -> TextureHandle;
    auto clear_history_resources() -> void;

    float3 position = float3{0.0f, 0.0f, -1.0f};
    float3 front_dir = float3{0.0f, 0.0f, 1.0f};
    float3 up_dir = float3{0.0f, 1.0f, 0.0f};
    float yfov = 30.0f;
    float near_z = 0.01f;
    float far_z = 10000.0f;
    ProjectionType projection_type = ProjectionType::perspective;

    bool enabled = true;

private:
    friend GraphicsManager;

    Texture target_texture_;

    ShaderParameter shader_parameter_;
    float4x4 matrix_proj_view_ = float4x4(1.0f);
    float4x4 history_matrix_proj_view_ = float4x4(1.0f);

    mutable StringHashMap<Box<Buffer>> history_buffers_[2];
    mutable StringHashMap<Box<Texture>> history_textures_[2];
    size_t history_index_ = 0;
};

}
