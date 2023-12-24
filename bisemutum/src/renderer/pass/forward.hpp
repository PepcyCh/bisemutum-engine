#pragma once

#include <bisemutum/graphics/render_graph.hpp>

#include "../context/lights.hpp"
#include "../context/skybox.hpp"
#include "skybox_precompute.hpp"

namespace bi {

struct ForwardPass final {
    struct InputData final {
        gfx::TextureHandle dir_lighst_shadow_map;
        PrecomputedSkybox skybox;
    };

    struct OutputData final {
        gfx::TextureHandle output;
        gfx::TextureHandle depth;
    };

    ForwardPass();

    auto update_params(LightsContext& lights_ctx, SkyboxContext& skybox_ctx) -> void;

    auto render(gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input) -> OutputData;

    gfx::FragmentShader fragmeng_shader;
    gfx::ShaderParameter fragmeng_shader_params;

    gfx::FragmentShader skybox_shader;
    gfx::ShaderParameter skybox_shader_params;
};

}
