#pragma once

#include "../prelude/idiom.hpp"
#include "../rhi/command.hpp"
#include "../graphics/resource.hpp"

namespace bi {

struct Window;

namespace gfx {

struct GraphicsManager;

}

struct ImGuiRenderer final : PImpl<ImGuiRenderer> {
    struct Impl;

    ImGuiRenderer();
    ~ImGuiRenderer();

    auto initialize(Window& window, gfx::GraphicsManager& gfx_mgr) -> void;
    auto finalize() -> void;

    auto new_frame() -> void;

    auto render_draw_data(Ref<rhi::CommandEncoder> cmd_encoder, Ref<gfx::Texture> target_texture) -> void;
};

}
