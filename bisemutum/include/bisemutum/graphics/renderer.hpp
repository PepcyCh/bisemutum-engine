#pragma once

#include "../prelude/poly.hpp"
#include "../rhi/descriptor.hpp"

namespace bi::gfx {

struct Camera;

BI_TRAIT_BEGIN(IRenderer, move)
    BI_TRAIT_METHOD(renderer_type_name, (&self) requires (self.renderer_type_name()) -> std::string_view)
    BI_TRAIT_METHOD(bind_group_layout, (const& self) requires (self.bind_group_layout()) -> rhi::BindGroupLayout const&)
    BI_TRAIT_METHOD(prepare_renderer_per_frame_data,
        (&self) requires (self.prepare_renderer_per_frame_data()) -> void
    )
    BI_TRAIT_METHOD(prepare_renderer_per_camera_data,
        (&self, CRef<Camera> camera) requires (self.prepare_renderer_per_camera_data(camera)) -> void
    )
    BI_TRAIT_METHOD(render_camera, (&self, CRef<Camera> camera) requires (self.render_camera(camera)) -> void)
BI_TRAIT_END(IRenderer)

}
