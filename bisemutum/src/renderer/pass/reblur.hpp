#pragma once

#include <bisemutum/graphics/render_graph.hpp>

#include "../context/gbuffer.hpp"

namespace bi {

struct ReblurPass final {
    struct InputData final {
        gfx::TextureHandle velocity;
        gfx::TextureHandle depth;
        GBufferTextures gbuffer;
        gfx::TextureHandle history_validation;

        gfx::TextureHandle hit_positions_tex;
        gfx::TextureHandle noised_tex;
    };

    ReblurPass();

    auto render(gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input) -> gfx::TextureHandle;

private:
    gfx::ComputeShader pre_blur_shader_;
    gfx::ShaderParameter pre_blur_shader_params_;
    gfx::ComputeShader temporal_accumulate_shader_;
    gfx::ShaderParameter temporal_accumulate_shader_params_;
    gfx::ComputeShader fetch_linear_depth_shader_;
    gfx::ShaderParameter fetch_linear_depth_shader_params_;
    gfx::ComputeShader gen_depth_mip_shader_;
    gfx::ShaderParameter gen_depth_mip_shader_params_;
    gfx::ComputeShader fix_history_shader_;
    gfx::ShaderParameter fix_history_shader_params_;
    gfx::ComputeShader blur_shader_;
    gfx::ShaderParameter blur_shader_params_;
    gfx::ComputeShader temporal_stabilize_shader_;
    gfx::ShaderParameter temporal_stabilize_shader_params_;
    gfx::ComputeShader post_blur_shader_;
    gfx::ShaderParameter post_blur_shader_params_;

    Ptr<gfx::Sampler> sampler_;

    std::unordered_map<gfx::Camera const*, uint64_t> last_frame_counts_;

    bool half_resolution_ = false;
};

}
