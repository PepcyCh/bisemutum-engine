#pragma once

#include <bisemutum/graphics/render_graph.hpp>

namespace bi {

struct ValidateHistoryPass final {
    struct InputData final {
        gfx::TextureHandle velocity;
        gfx::TextureHandle depth;
        gfx::TextureHandle normal_roughness;
    };

    ValidateHistoryPass();

    auto render(gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input) -> gfx::TextureHandle;

private:
    gfx::ComputeShader compute_shader_;
    gfx::ShaderParameter compute_shader_params_;

    std::unordered_map<gfx::Camera const*, uint64_t> last_frame_counts_;
};

}
