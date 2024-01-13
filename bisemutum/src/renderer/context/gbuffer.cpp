#include "gbuffer.hpp"

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>

namespace bi {

auto GBufferTextures::add_textures(gfx::RenderGraph& rg, uint32_t width, uint32_t height) -> void {
    base_color = rg.add_texture([width, height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(rhi::ResourceFormat::rgba16_sfloat, width, height)
            .usage({rhi::TextureUsage::color_attachment, rhi::TextureUsage::sampled});
    });
    normal_roughness = rg.add_texture([width, height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(rhi::ResourceFormat::rgba16_sfloat, width, height)
            .usage({rhi::TextureUsage::color_attachment, rhi::TextureUsage::sampled});
    });
    specular_model = rg.add_texture([width, height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(rhi::ResourceFormat::rgba16_sfloat, width, height)
            .usage({rhi::TextureUsage::color_attachment, rhi::TextureUsage::sampled});
    });
    additional_0 = rg.add_texture([width, height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(rhi::ResourceFormat::rgba8_unorm, width, height)
            .usage({rhi::TextureUsage::color_attachment, rhi::TextureUsage::sampled});
    });
}

auto GBufferTextures::use_color(gfx::GraphicsPassBuilder& builder) -> void {
    builder.use_color(0, gfx::GraphicsPassColorTargetBuilder{base_color}.clear_color());
    builder.use_color(1, gfx::GraphicsPassColorTargetBuilder{normal_roughness}.clear_color());
    builder.use_color(2, gfx::GraphicsPassColorTargetBuilder{specular_model}.clear_color());
    builder.use_color(3, gfx::GraphicsPassColorTargetBuilder{additional_0}.clear_color());
}

auto GBufferTextures::read(gfx::GraphicsPassBuilder& builder) const -> GBufferTextures {
    GBufferTextures ret;
    ret.base_color = builder.read(base_color);
    ret.normal_roughness = builder.read(normal_roughness);
    ret.specular_model = builder.read(specular_model);
    ret.additional_0 = builder.read(additional_0);
    return ret;
}

auto GBufferTextures::get_sampler() const -> Ptr<gfx::Sampler> {
    return g_engine->graphics_manager()->get_sampler(rhi::SamplerDesc{
        .mag_filter = rhi::SamplerFilterMode::linear,
        .min_filter = rhi::SamplerFilterMode::linear,
        .address_mode_u = rhi::SamplerAddressMode::clamp_to_edge,
        .address_mode_v = rhi::SamplerAddressMode::clamp_to_edge,
        .address_mode_w = rhi::SamplerAddressMode::clamp_to_edge,
    });
}

}
