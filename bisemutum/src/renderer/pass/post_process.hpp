#pragma once

#include <bisemutum/graphics/render_graph.hpp>

namespace bi {

struct PostProcessPass final {
    struct InputData final {
        gfx::TextureHandle color;
        gfx::TextureHandle depth;
    };

    PostProcessPass();

    auto render(gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input) -> void;

    gfx::FragmentShader fragmeng_shader;
    gfx::ShaderParameter fragmeng_shader_params;

    gfx::FragmentShader bloom_pre_shader;
    gfx::ShaderParameter bloom_pre_shader_params;
    gfx::FragmentShader bloom_filter_shader[2];
    std::vector<gfx::ShaderParameter> bloom_filter_shader_params;
    gfx::FragmentShader bloom_combine_shader;
    std::vector<gfx::ShaderParameter> bloom_combine_shader_params;

    Ptr<gfx::Sampler> sampler;
};

}
