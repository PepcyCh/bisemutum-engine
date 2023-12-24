#include "skybox.hpp"

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/graphics/resource_builder.hpp>

namespace bi {

SkyboxContext::SkyboxContext() {
    diffuse_irradiance = gfx::Texture(
        gfx::TextureBuilder{}
            .dim_cube(rhi::ResourceFormat::rgba16_sfloat, 1024)
            .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write})
    );

    specular_filtered = gfx::Texture(
        gfx::TextureBuilder{}
            .dim_cube(rhi::ResourceFormat::rgba16_sfloat, 1024)
            .mipmap(5) // ibl_specular_num_levels
            .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write})
    );

    brdf_lut = gfx::Texture(
        gfx::TextureBuilder{}
            .dim_2d(rhi::ResourceFormat::rg8_unorm, 1024, 1024)
            .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write})
    );

    skybox_sampler = g_engine->graphics_manager()->get_sampler(rhi::SamplerDesc{
        .mag_filter = rhi::SamplerFilterMode::linear,
        .min_filter = rhi::SamplerFilterMode::linear,
        .address_mode_u = rhi::SamplerAddressMode::clamp_to_edge,
        .address_mode_v = rhi::SamplerAddressMode::clamp_to_edge,
        .address_mode_w = rhi::SamplerAddressMode::clamp_to_edge,
    });
}

}
