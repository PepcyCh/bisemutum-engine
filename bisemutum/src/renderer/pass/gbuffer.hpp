#pragma once

#include <bisemutum/graphics/render_graph.hpp>

#include "../context/gbuffer.hpp"

namespace bi {

struct GBufferdPass final {
    struct OutputData final {
        gfx::TextureHandle color;
        gfx::TextureHandle depth;
        gfx::TextureHandle velocity;
        GBufferTextures gbuffer;
    };

    GBufferdPass();

    auto render(gfx::Camera const& camera, gfx::RenderGraph& rg, Span<Ref<gfx::Drawable>> drawables) -> OutputData;

private:
    gfx::FragmentShader fragment_shader_;
    gfx::ShaderParameter fragment_shader_params_;
};

}
