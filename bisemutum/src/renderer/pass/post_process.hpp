#pragma once

#include <bisemutum/graphics/render_graph.hpp>

namespace bi {

struct PostProcessPass final {
    struct InputData final {
        gfx::TextureHandle color;
        gfx::TextureHandle depth;
    };

    struct PassData final {
        gfx::TextureHandle input_color;
        gfx::TextureHandle input_depth;

        gfx::TextureHandle output;
    };

    PostProcessPass();

    auto render(gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input) -> Ref<PassData>;

    gfx::FragmentShader fragmeng_shader;
    gfx::ShaderParameter fragmeng_shader_params;

    Ptr<gfx::Sampler> sampler;
};

}
