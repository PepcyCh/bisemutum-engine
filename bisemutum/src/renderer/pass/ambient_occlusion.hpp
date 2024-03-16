#pragma once

#include <bisemutum/graphics/render_graph.hpp>
#include <bisemutum/renderer/basic.hpp>

namespace bi {

struct AmbientOcclusionPass final {
    struct InputData final {
        gfx::TextureHandle color;
        gfx::TextureHandle depth;
        gfx::TextureHandle normal_roughness;
        gfx::TextureHandle velocity;

        gfx::AccelerationStructureHandle scene_accel;
    };

    AmbientOcclusionPass();

    auto render(
        gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input,
        BasicRenderer::AmbientOcclusionSettings const& settings
    ) -> gfx::TextureHandle;

private:
    auto render_screen_space(
        gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input,
        BasicRenderer::AmbientOcclusionSettings const& settings
    ) -> gfx::TextureHandle;

    auto render_raytraced(
        gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input,
        BasicRenderer::AmbientOcclusionSettings const& settings
    ) -> gfx::TextureHandle;

    auto render_denoise(
        gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input,
        BasicRenderer::AmbientOcclusionSettings const& settings,
        gfx::TextureHandle ao_tex
    ) -> gfx::TextureHandle;

    auto render_merge(
        gfx::Camera const& camera, gfx::RenderGraph& rg,
        gfx::TextureHandle color_tex, gfx::TextureHandle ao_tex
    ) -> gfx::TextureHandle;

    gfx::FragmentShader screen_space_shader_;
    gfx::ShaderParameter screen_space_shader_params_;

    gfx::ComputeShader raytracing_shader_;
    gfx::ShaderParameter raytracing_shader_params_;

    gfx::ComputeShader temporal_accumulate_shader_;
    gfx::ShaderParameter temporal_accumulate_shader_params_;

    gfx::ComputeShader spatial_filter_shader_;
    gfx::ShaderParameter spatial_filter_shader_params_;

    gfx::FragmentShader merge_ao_shader_;
    gfx::ShaderParameter merge_ao_shader_params_;

    Ptr<gfx::Sampler> sampler_;

    std::unordered_map<gfx::Camera const*, uint64_t> last_frame_counts_;
};

}
