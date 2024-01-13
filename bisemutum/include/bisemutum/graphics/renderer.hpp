#pragma once

#include "../prelude/poly.hpp"

namespace bi::gfx {

struct Camera;
struct RenderGraph;

BI_TRAIT_BEGIN(IRenderer, move)
    BI_TRAIT_METHOD(override_volume_component_name,
        (const& self) requires (self.override_volume_component_name()) -> std::string_view
    )
    BI_TRAIT_METHOD(prepare_renderer_per_frame_data,
        (&self) requires (self.prepare_renderer_per_frame_data()) -> void
    )
    BI_TRAIT_METHOD(prepare_renderer_per_camera_data,
        (&self, Camera const& camera) requires (self.prepare_renderer_per_camera_data(camera)) -> void
    )
    BI_TRAIT_METHOD(render_camera,
        (&self, Camera const& camera, RenderGraph& rg) requires (self.render_camera(camera, rg)) -> void
    )
BI_TRAIT_END(IRenderer)

}
