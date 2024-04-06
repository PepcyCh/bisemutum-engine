#pragma once

#include <bisemutum/graphics/render_graph.hpp>

namespace bi {

struct GBufferTextures final {
    gfx::TextureHandle base_color;
    gfx::TextureHandle normal_roughness;
    gfx::TextureHandle fresnel;
    gfx::TextureHandle material_0;

    static inline constexpr uint32_t count = 4;
    static inline constexpr rhi::ResourceFormat base_color_format = rhi::ResourceFormat::rgba16_sfloat;
    static inline constexpr rhi::ResourceFormat normal_roughness_format = rhi::ResourceFormat::rgba16_sfloat;
    static inline constexpr rhi::ResourceFormat fresnel_format = rhi::ResourceFormat::rgba16_unorm;
    static inline constexpr rhi::ResourceFormat material_0_format = rhi::ResourceFormat::rgba8_unorm;

    auto add_textures(
        gfx::RenderGraph& rg, uint32_t width, uint32_t height,
        BitFlags<rhi::TextureUsage> usages = {rhi::TextureUsage::color_attachment, rhi::TextureUsage::sampled}
    ) -> void;

    auto use_color(gfx::GraphicsPassBuilder& builder) -> void;

    auto write(gfx::ComputePassBuilder& builder) const -> GBufferTextures;
    auto write(gfx::RaytracingPassBuilder& builder) const -> GBufferTextures;

    auto read(gfx::GraphicsPassBuilder& builder) const -> GBufferTextures;
    auto read(gfx::ComputePassBuilder& builder) const -> GBufferTextures;
    auto read(gfx::RaytracingPassBuilder& builder) const -> GBufferTextures;

    auto get_sampler() const -> Ptr<gfx::Sampler>;
};

}
