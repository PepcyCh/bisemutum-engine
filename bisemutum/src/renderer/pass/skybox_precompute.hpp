#pragma once

#include <bisemutum/graphics/render_graph.hpp>

#include "../context/skybox.hpp"

namespace bi {

struct PrecomputedSkybox final {
    gfx::TextureHandle skybox = gfx::TextureHandle::invalid;
    gfx::TextureHandle diffuse_irradiance = gfx::TextureHandle::invalid;
    gfx::TextureHandle specular_filtered = gfx::TextureHandle::invalid;
    gfx::TextureHandle brdf_lut = gfx::TextureHandle::invalid;
};

struct SkyboxPrecomputePass final {
    struct InputData final {
        SkyboxContext& skybox_ctx;
    };

    SkyboxPrecomputePass();

    auto render(gfx::RenderGraph& rg, InputData const& input) -> PrecomputedSkybox;

private:
    gfx::ComputeShader precompute_brdf_lut_;
    gfx::ShaderParameter precompute_brdf_lut_params_;

    gfx::ComputeShader precompute_diffuse_;
    gfx::ShaderParameter precompute_diffuse_params_;

    gfx::ComputeShader precompute_specular_;
    // TODO - support push constants in shader params
    std::vector<gfx::ShaderParameter> precompute_specular_params_;

    Ptr<gfx::Texture> last_precomputed_skybox_;
    bool brdf_lut_valid_ = false;
};

}
