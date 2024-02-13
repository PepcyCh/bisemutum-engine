#pragma once

#include <bisemutum/graphics/render_graph.hpp>
#include <bisemutum/scene_basic/post_process_volume.hpp>

namespace bi {

struct PostProcessPass final {
    struct InputData final {
        gfx::TextureHandle color;
        gfx::TextureHandle depth;
    };

    PostProcessPass();

    auto render(gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input) -> void;

private:
    gfx::FragmentShader fragment_shader_;
    gfx::ShaderParameter fragment_shader_params_;

    gfx::FragmentShader bloom_pre_shader_;
    gfx::ShaderParameter bloom_pre_shader_params_;
    gfx::FragmentShader bloom_filter_shader_[2];
    std::vector<gfx::ShaderParameter> bloom_filter_shader_params_;
    gfx::FragmentShader bloom_combine_shader_;
    std::vector<gfx::ShaderParameter> bloom_combine_shader_params_;

    Ptr<gfx::Sampler> sampler_;

    PostProcessVolumeComponent default_volume_;
};

}
