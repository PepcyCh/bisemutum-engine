#pragma once

#include <bisemutum/graphics/render_graph.hpp>

namespace bi {

struct GBufferTextures final {
    gfx::TextureHandle base_color;
    gfx::TextureHandle normal_roughness;
    gfx::TextureHandle specular_model;
    gfx::TextureHandle additional_0;

    static inline constexpr uint32_t count = 4;

    auto add_textures(gfx::RenderGraph& rg, uint32_t width, uint32_t height) -> void;

    auto use_color(gfx::GraphicsPassBuilder& builder) -> void;

    auto read(gfx::GraphicsPassBuilder& builder) const -> GBufferTextures;

    auto get_sampler() const -> Ptr<gfx::Sampler>;
};

}
