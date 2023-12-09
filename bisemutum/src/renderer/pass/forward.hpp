#pragma once

#include <bisemutum/graphics/render_graph.hpp>

#include "../context/lights.hpp"

namespace bi {

struct ForwardPass final {
    struct InputData final {
        gfx::TextureHandle dir_lighst_shadow_map;
    };

    struct PassData final {
        gfx::TextureHandle output;
        gfx::TextureHandle depth;

        gfx::RenderedObjectListHandle list;
    };

    ForwardPass();

    auto update_params(LightsContext& lights_ctx) -> void;

    auto render(gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input) -> Ref<PassData>;

    gfx::FragmentShader fragmeng_shader;
    gfx::ShaderParameter fragmeng_shader_params;
};

}
