#pragma once

#include <bisemutum/graphics/render_graph.hpp>
#include <bisemutum/renderer/basic.hpp>

namespace bi {

struct DepthPyramidPass final {
    struct InputData final {
        gfx::TextureHandle depth;
    };

    DepthPyramidPass();

    auto render(gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input) -> gfx::TextureHandle;

private:
    gfx::ComputeShader depth_pyramid_shader_1_;
    gfx::ShaderParameter depth_pyramid_shader_1_params_;

    gfx::ComputeShader depth_pyramid_shader_2_;
    gfx::ShaderParameter depth_pyramid_shader_2_params_;

    Ptr<gfx::Sampler> sampler_;
};

}
