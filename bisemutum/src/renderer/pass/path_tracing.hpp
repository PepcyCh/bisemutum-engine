#pragma once

#include <bisemutum/graphics/render_graph.hpp>
#include <bisemutum/renderer/basic.hpp>

#include "../context/lights.hpp"
#include "../context/skybox.hpp"
#include "../context/gbuffer.hpp"

namespace bi {

struct PathTracingPass final {
    struct InputData final {
        ShadowMapTextures shadow_maps;
        gfx::AccelerationStructureHandle scene_accel;
    };

    struct OutputData final {
        gfx::TextureHandle color;
        gfx::TextureHandle depth;
        gfx::TextureHandle velocity;
        GBufferTextures gbuffer;
    };

    PathTracingPass();

    auto update_params(
        LightsContext& lights_ctx, SkyboxContext& skybox_ctx,
        BasicRenderer::PathTracingSettings const& settings
    ) -> void;

    auto render(
        gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input,
        BasicRenderer::PathTracingSettings const& settings
    ) -> OutputData;

private:
    gfx::ComputeShader primary_ray_gen_shader_;
    gfx::ShaderParameter primary_ray_gen_shader_params_;

    gfx::ComputeShader direction_sample_shader_;
    std::vector<gfx::ShaderParameter> direction_sample_shader_params_;

    gfx::RaytracingShaders rt_gbuffer_shader_;
    std::vector<gfx::ShaderParameter> rt_gbuffer_shader_params_;

    gfx::ComputeShader rt_deferred_lighting_shader_;
    std::vector<gfx::ShaderParameter> rt_deferred_lighting_shader_params_;

    gfx::FragmentShader blit_color_shader_;
    std::vector<gfx::ShaderParameter> blit_color_shader_params_;

    gfx::FragmentShader pt_depth_shader_;
    gfx::ShaderParameter pt_depth_shader_params_;

    gfx::ComputeShader accumulate_shader_;
    gfx::ShaderParameter accumulate_shader_params_;

    Ptr<gfx::Sampler> sampler_;

    struct CameraHistoryInfo final {
        uint64_t last_frame;
        uint64_t frame_count;
        float4x4 proj_view;
        uint32_t width;
        uint32_t height;
    };
    std::unordered_map<gfx::Camera const*, CameraHistoryInfo> camera_history_infos_;
};

}
