#pragma once

#include <bisemutum/graphics/render_graph.hpp>

#include "../context/lights.hpp"

namespace bi {

struct ShadowMappingPass final {
    struct InputData final {
        LightsContext& lights_ctx;
    };

    ShadowMappingPass();

    auto render(gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input) -> gfx::TextureHandle;

    gfx::FragmentShader fragment_shader;
    gfx::ShaderParameter fragment_shader_params;
};

}
