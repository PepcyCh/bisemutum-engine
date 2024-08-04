#pragma once

#include <bisemutum/graphics/render_graph.hpp>
#include <bisemutum/renderer/post_process_volume.hpp>

namespace bi {

struct RasterizationOrderPass final {
    RasterizationOrderPass();

    auto render(gfx::Camera const& camera, gfx::RenderGraph& rg) -> void;

private:
    gfx::ComputeShader initialize_shader_;
    gfx::ShaderParameter initialize_shader_params_;

    gfx::FragmentShader fragment_shader_;
    gfx::ShaderParameter fragment_shader_params_;

    Ptr<gfx::Sampler> sampler_;
};

}
