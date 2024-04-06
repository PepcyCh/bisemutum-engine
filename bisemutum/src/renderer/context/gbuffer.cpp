#include "gbuffer.hpp"

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>

namespace bi {

auto GBufferTextures::add_textures(
    gfx::RenderGraph& rg, uint32_t width, uint32_t height, BitFlags<rhi::TextureUsage> usages
) -> void {
    base_color = rg.add_texture([width, height, usages](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(base_color_format, width, height)
            .usage(usages);
    });
    normal_roughness = rg.add_texture([width, height, usages](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(normal_roughness_format, width, height)
            .usage(usages);
    });
    fresnel = rg.add_texture([width, height, usages](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(fresnel_format, width, height)
            .usage(usages);
    });
    material_0 = rg.add_texture([width, height, usages](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(material_0_format, width, height)
            .usage(usages);
    });
}

auto GBufferTextures::use_color(gfx::GraphicsPassBuilder& builder) -> void {
    base_color = builder.use_color(0, gfx::GraphicsPassColorTargetBuilder{base_color}.clear_color());
    normal_roughness = builder.use_color(1, gfx::GraphicsPassColorTargetBuilder{normal_roughness}.clear_color());
    fresnel = builder.use_color(2, gfx::GraphicsPassColorTargetBuilder{fresnel}.clear_color());
    material_0 = builder.use_color(3, gfx::GraphicsPassColorTargetBuilder{material_0}.clear_color());
}

auto GBufferTextures::write(gfx::ComputePassBuilder& builder) const -> GBufferTextures {
    GBufferTextures ret;
    ret.base_color = builder.write(base_color);
    ret.normal_roughness = builder.write(normal_roughness);
    ret.fresnel = builder.write(fresnel);
    ret.material_0 = builder.write(material_0);
    return ret;
}
auto GBufferTextures::write(gfx::RaytracingPassBuilder& builder) const -> GBufferTextures {
    GBufferTextures ret;
    ret.base_color = builder.write(base_color);
    ret.normal_roughness = builder.write(normal_roughness);
    ret.fresnel = builder.write(fresnel);
    ret.material_0 = builder.write(material_0);
    return ret;
}

auto GBufferTextures::read(gfx::GraphicsPassBuilder& builder) const -> GBufferTextures {
    GBufferTextures ret;
    ret.base_color = builder.read(base_color);
    ret.normal_roughness = builder.read(normal_roughness);
    ret.fresnel = builder.read(fresnel);
    ret.material_0 = builder.read(material_0);
    return ret;
}
auto GBufferTextures::read(gfx::ComputePassBuilder& builder) const -> GBufferTextures {
    GBufferTextures ret;
    ret.base_color = builder.read(base_color);
    ret.normal_roughness = builder.read(normal_roughness);
    ret.fresnel = builder.read(fresnel);
    ret.material_0 = builder.read(material_0);
    return ret;
}
auto GBufferTextures::read(gfx::RaytracingPassBuilder& builder) const -> GBufferTextures {
    GBufferTextures ret;
    ret.base_color = builder.read(base_color);
    ret.normal_roughness = builder.read(normal_roughness);
    ret.fresnel = builder.read(fresnel);
    ret.material_0 = builder.read(material_0);
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
