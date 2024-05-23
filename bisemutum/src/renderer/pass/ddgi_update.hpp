#pragma once

#include <bisemutum/graphics/render_graph.hpp>

#include "../context/ddgi.hpp"
#include "../context/lights.hpp"
#include "../context/skybox.hpp"

namespace bi {

struct DdgiUpdatePass final {
    struct InputData final {
        DdgiContext& ddgi_ctx;
        ShadowMapTextures shadow_maps;
        gfx::AccelerationStructureHandle scene_accel;
    };
    struct OutputData final {
        gfx::TextureHandle irradiance;
        gfx::TextureHandle visibility;
    };

    DdgiUpdatePass();

    auto update_params(
        DdgiContext& ddgi_ctx, LightsContext& lights_ctx, SkyboxContext& skybox_ctx
    ) -> void;

    auto render(gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input) -> OutputData;

private:
    gfx::RaytracingShaders trace_gbuffer_shader_;
    std::vector<gfx::ShaderParameter> trace_gbuffer_shader_params_;

    gfx::ComputeShader deferred_lighting_shader_;
    std::vector<gfx::ShaderParameter> deferred_lighting_shader_params_;

    gfx::ComputeShader probe_blend_irradiance_shader_;
    std::vector<gfx::ShaderParameter> probe_blend_irradiance_shader_params_;

    gfx::ComputeShader probe_blend_visibility_shader_;
    std::vector<gfx::ShaderParameter> probe_blend_visibility_shader_params_;

    uint64_t last_frame_;
    Box<gfx::Texture> irradiance_texture_;
    Box<gfx::Texture> visibility_texture_;
};

}
