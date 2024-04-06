#pragma once

#include <bisemutum/graphics/render_graph.hpp>
#include <bisemutum/renderer/basic.hpp>

#include "../context/gbuffer.hpp"
#include "../context/lights.hpp"
#include "../context/skybox.hpp"
#include "skybox_precompute.hpp"

namespace bi {

struct ReflectionPass final {
    struct InputData final {
        gfx::TextureHandle color;
        gfx::TextureHandle velocity;
        gfx::TextureHandle depth;
        GBufferTextures gbuffer;

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
    ) -> gfx::TextureHandle;

    auto render_raytraced(
        gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input,
        BasicRenderer::ReflectionSettings const& settings
    ) -> gfx::TextureHandle;

    auto render_denoise(
        gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input,
        BasicRenderer::ReflectionSettings const& settings,
        gfx::TextureHandle reflection_tex
    ) -> gfx::TextureHandle;

    auto render_merge(
        gfx::Camera const& camera, gfx::RenderGraph& rg,
        gfx::TextureHandle color_tex, gfx::TextureHandle reflection_tex
    ) -> gfx::TextureHandle;

    gfx::FragmentShader screen_space_shader_;
    gfx::ShaderParameter screen_space_shader_params_;

    gfx::ComputeShader rt_direction_sample_shader_;
    gfx::ShaderParameter rt_direction_sample_shader_params_;

    gfx::RaytracingShaders rt_gbuffer_shader_;
    gfx::ShaderParameter rt_gbuffer_shader_params_;

    gfx::ComputeShader rt_deferred_lighting_shader_;
    gfx::ShaderParameter rt_deferred_lighting_shader_params_;

    gfx::FragmentShader merge_reflection_shader_;
    gfx::ShaderParameter merge_reflection_shader_params_;

    Ptr<gfx::Sampler> sampler_;

    std::unordered_map<gfx::Camera const*, uint64_t> last_frame_counts_;

    GBufferTextures hit_gbuffer_texs_;
    gfx::TextureHandle hit_positions_tex_;
};

}
