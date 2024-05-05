#pragma once

#include <bisemutum/graphics/render_graph.hpp>
#include <bisemutum/renderer/basic.hpp>

#include "../context/gbuffer.hpp"
#include "../context/lights.hpp"
#include "../context/skybox.hpp"
#include "skybox_precompute.hpp"
#include "reblur.hpp"

namespace bi {

struct ReflectionPass final {
    struct InputData final {
        gfx::TextureHandle color;
        gfx::TextureHandle velocity;
        gfx::TextureHandle depth;
        gfx::TextureHandle depth_pyramid;
        GBufferTextures gbuffer;
        gfx::TextureHandle history_validation;

        ShadowMapTextures shadow_maps;
        PrecomputedSkybox skybox;

        gfx::AccelerationStructureHandle scene_accel;
    };

    ReflectionPass();

    auto update_params(LightsContext& lights_ctx, SkyboxContext& skybox_ctx) -> void;

    auto render(
        gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input,
        BasicRenderer::ReflectionSettings const& settings
    ) -> gfx::TextureHandle;

private:
    auto render_screen_space(
        gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input,
        BasicRenderer::ReflectionSettings const& settings
    ) -> void;

    auto render_raytraced(
        gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input,
        BasicRenderer::ReflectionSettings const& settings
    ) -> void;

    auto render_upscale(
        gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input
    ) -> void;

    auto render_denoise(
        gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input,
        BasicRenderer::ReflectionSettings const& settings,
        gfx::TextureHandle reflection_tex
    ) -> gfx::TextureHandle;

    auto render_merge(
        gfx::Camera const& camera, gfx::RenderGraph& rg,
        gfx::TextureHandle color_tex, gfx::TextureHandle reflection_tex
    ) -> gfx::TextureHandle;

    gfx::ComputeShader screen_space_shader_;
    gfx::ShaderParameter screen_space_shader_params_;

    gfx::ComputeShader rt_direction_sample_shader_;
    gfx::ShaderParameter rt_direction_sample_shader_params_;

    gfx::RaytracingShaders rt_gbuffer_shader_;
    gfx::ShaderParameter rt_gbuffer_shader_params_;

    gfx::ComputeShader rt_deferred_lighting_shader_;
    gfx::ShaderParameter rt_deferred_lighting_shader_params_;

    gfx::ComputeShader simple_upscale_hit_shader_;
    gfx::ShaderParameter simple_upscale_hit_shader_params_;
    gfx::ComputeShader simple_upscale_color_shader_;
    gfx::ShaderParameter simple_upscale_color_shader_params_;

    gfx::FragmentShader merge_reflection_shader_;
    gfx::ShaderParameter merge_reflection_shader_params_;

    Ptr<gfx::Sampler> sampler_;

    std::unordered_map<gfx::Camera const*, uint64_t> last_frame_counts_;

    gfx::TextureHandle reflection_tex_;
    gfx::TextureHandle hit_positions_tex_;

    ReblurPass reblur_pass_;
};

}
