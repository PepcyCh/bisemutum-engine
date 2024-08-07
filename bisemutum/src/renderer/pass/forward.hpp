#pragma once

#include <bisemutum/graphics/render_graph.hpp>

#include "../context/lights.hpp"
#include "../context/skybox.hpp"
#include "skybox_precompute.hpp"
#include "ddgi_update.hpp"

namespace bi {

struct ForwardPass final {
    struct InputData final {
        Span<Ref<gfx::Drawable>> drawables;
        ShadowMapTextures shadow_maps;
        PrecomputedSkybox skybox;
        DdgiTextures ddgi;

        DdgiContext const& ddgi_ctx;
    };

    struct OutputData final {
        gfx::TextureHandle output;
        gfx::TextureHandle velocity;
        gfx::TextureHandle depth;
    };

    ForwardPass();

    auto update_params(LightsContext& lights_ctx, SkyboxContext& skybox_ctx) -> void;

    auto render(gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input) -> OutputData;

private:
    gfx::FragmentShader fragment_shader_;
    gfx::ShaderParameter fragment_shader_params_;
};

struct ForwardTransparentPass final {
    struct InputData final {
        Span<Ref<gfx::Drawable>> drawables;
        gfx::TextureHandle color;
        gfx::TextureHandle depth;
        ShadowMapTextures shadow_maps;
        PrecomputedSkybox skybox;
    };

    struct OutputData final {
        gfx::TextureHandle color;
        gfx::TextureHandle depth;
    };

    ForwardTransparentPass();

    auto update_params(LightsContext& lights_ctx, SkyboxContext& skybox_ctx) -> void;

    auto render(gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input) -> OutputData;

private:
    gfx::FragmentShader fragment_shader_;
    gfx::ShaderParameter fragment_shader_params_;
};

}
