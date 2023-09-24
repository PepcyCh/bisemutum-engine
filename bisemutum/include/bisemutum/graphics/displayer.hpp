#pragma once

#include "resource.hpp"
#include "handles.hpp"
#include "../prelude/poly.hpp"
#include "../rhi/command.hpp"

namespace bi::gfx {

BI_TRAIT_BEGIN(IDisplayer, move)
    BI_TRAIT_METHOD(display,
        (&self, Ref<rhi::CommandEncoder> cmd_encoder, Ref<Texture> target_texture)
            requires (self.display(cmd_encoder, target_texture)) -> void
    )
    BI_TRAIT_METHOD(is_valid, (const& self) requires (self.is_valid()) -> bool)
BI_TRAIT_END(IDisplayer)

struct BlitDisplayer final {
    auto display(Ref<rhi::CommandEncoder> cmd_encoder, Ref<Texture> target_texture) -> void;

    auto is_valid() const -> bool;

    auto set_camera(CameraHandle camera) -> void;

private:
    CameraHandle displayed_camera_ = CameraHandle::invalid;
};

}
