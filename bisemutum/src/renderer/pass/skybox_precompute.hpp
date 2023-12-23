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

    gfx::ComputeShader precompute_diffuse;
    gfx::ShaderParameter precompute_diffuse_params;
    gfx::ComputeShader precompute_specular;
    gfx::ShaderParameter precompute_specular_params;

    Option<rt::AssetId> last_precomputed_asset;
};

}
