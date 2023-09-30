#pragma once

#include <bisemutum/graphics/render_graph.hpp>

#include "../context/lights.hpp"

namespace bi {

struct ForwardPass final {
    struct PassData final {
        gfx::TextureHandle output;
        gfx::TextureHandle depth;

        gfx::RenderedObjectListHandle list;
    };

    ForwardPass();

    auto update_params(LightsContext& lights_ctx) -> void;

    auto render(gfx::Camera const& camera, gfx::RenderGraph& rg) -> Ref<PassData>;

    gfx::FragmentShader fragmeng_shader;
    gfx::ShaderParameter fragmeng_shader_params;
};

}
