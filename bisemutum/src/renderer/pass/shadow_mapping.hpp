#pragma once

#include <bisemutum/graphics/render_graph.hpp>

#include "../context/lights.hpp"

namespace bi {

struct ShadowMappingPass final {
    struct InputData final {
        Span<Ref<gfx::Drawable>> drawables;
        LightsContext& lights_ctx;
    };

    ShadowMappingPass();

    auto render(gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input) -> ShadowMapTextures;

private:
    gfx::FragmentShader fragment_shader_;
    gfx::ShaderParameter fragment_shader_params_;
};

}
