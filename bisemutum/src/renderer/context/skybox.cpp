#include "skybox.hpp"

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/graphics/resource_builder.hpp>
#include <bisemutum/runtime/system_manager.hpp>
#include <bisemutum/scene_basic/skybox.hpp>

namespace bi {

SkyboxContext::SkyboxContext() {
    diffuse_irradiance = gfx::Texture(
        gfx::TextureBuilder{}
            .dim_cube(rhi::ResourceFormat::rgba16_sfloat, 256)
            .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write})
    );

    specular_filtered = gfx::Texture(
        gfx::TextureBuilder{}
            .dim_cube(rhi::ResourceFormat::rgba16_sfloat, 256)
            .mipmap(5) // ibl_specular_num_levels
            .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write})
    );

    brdf_lut = gfx::Texture(
        gfx::TextureBuilder{}
            .dim_2d(rhi::ResourceFormat::rg8_unorm, 128, 128)
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

auto SkyboxContext::update_shader_params() -> void {
    auto& current_skybox = g_engine->system_manager()->get_system_for_current_scene<SkyboxSystem>()->current_skybox();
    shader_data.skybox_diffuse_color = current_skybox.color * current_skybox.diffuse_strength;
    shader_data.skybox_specular_color = current_skybox.color * current_skybox.specular_strength;
    shader_data.skybox_transform = current_skybox.transform;

    shader_data.skybox_diffuse_irradiance = {&diffuse_irradiance};
    shader_data.skybox_specular_filtered = {&specular_filtered};
    shader_data.skybox_brdf_lut = {&brdf_lut};
    shader_data.skybox_sampler = {skybox_sampler};
}

}
