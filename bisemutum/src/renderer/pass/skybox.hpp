#pragma once

#include <bisemutum/graphics/render_graph.hpp>

#include "../context/skybox.hpp"
#include "skybox_precompute.hpp"

namespace bi {

struct SkyboxPass final {
    struct InputData final {
        gfx::TextureHandle color;
        gfx::TextureHandle depth;
        PrecomputedSkybox skybox;
    };
    struct OutputData final {
        gfx::TextureHandle color;
        gfx::TextureHandle depth;
    };

    SkyboxPass();

    auto update_params(SkyboxContext& skybox_ctx) -> void;

    auto render(gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input) -> OutputData;

private:
    gfx::FragmentShader fragment_shader_;
    gfx::ShaderParameter fragment_shader_params_;
};

}
