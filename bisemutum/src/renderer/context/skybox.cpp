#include "skybox.hpp"

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/world.hpp>
#include <bisemutum/runtime/scene.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/graphics/resource_builder.hpp>
#include <bisemutum/scene_basic/skybox.hpp>

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
            .mipmap()
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

auto SkyboxContext::find_skybox() -> void {
    auto scene = g_engine->world()->current_scene().value();
    auto skybox_view = scene->ecs_registry().view<SkyboxComponent>();
    skybox_texture_id = rt::AssetId::invalid;
    for (auto entity : skybox_view) {
        auto& skybox = skybox_view.get<SkyboxComponent>(entity);
        skybox.texture.load();
        skybox_tex = &skybox.texture.asset()->texture;
        skybox_texture_id = skybox.texture.asset_id();
    }

    if (skybox_texture_id == rt::AssetId::invalid) {
        skybox_tex = g_engine->graphics_manager()->default_texture(gfx::DefaultTexture::black_1x1_cube);
    }
}

}
