#pragma once

#include <bisemutum/graphics/render_graph.hpp>

#include "../context/gbuffer.hpp"
#include "../context/lights.hpp"
#include "../context/skybox.hpp"
#include "skybox_precompute.hpp"

namespace bi {

struct DeferredLightingPass final {
    struct InputData final {
        gfx::TextureHandle depth;
        GBufferTextures gbuffer;

        ShadowMapTextures shadow_maps;
        PrecomputedSkybox skybox;
    };

    struct OutputData final {
        gfx::TextureHandle output;
    };

    DeferredLightingPass();

    auto update_params(LightsContext& lights_ctx, SkyboxContext& skybox_ctx) -> void;

    auto render(gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input) -> OutputData;

private:
    gfx::FragmentShader fragment_shader_;
    gfx::ShaderParameter fragment_shader_params_;
};

}
