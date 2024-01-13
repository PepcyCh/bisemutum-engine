#pragma once

#include <bisemutum/graphics/render_graph.hpp>

#include "../context/gbuffer.hpp"

namespace bi {

struct GBufferdPass final {
    struct OutputData final {
        gfx::TextureHandle depth;
        GBufferTextures gbuffer;
    };

    GBufferdPass();

    auto render(gfx::Camera const& camera, gfx::RenderGraph& rg) -> OutputData;

    gfx::FragmentShader fragment_shader;
    gfx::ShaderParameter fragment_shader_params;
};

}
