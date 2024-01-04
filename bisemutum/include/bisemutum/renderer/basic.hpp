#pragma once

#include "../prelude/idiom.hpp"
#include "../graphics/camera.hpp"
#include "../graphics/render_graph.hpp"

namespace bi {

struct BasicRenderer final : PImpl<BasicRenderer> {
    struct Impl;

    BasicRenderer();
    ~BasicRenderer();

    BasicRenderer(BasicRenderer&& rhs) noexcept;
    auto operator=(BasicRenderer&& rhs) noexcept -> BasicRenderer&;

    static constexpr std::string_view renderer_type_name = "BasicRenderer";

    auto prepare_renderer_per_frame_data() -> void;
    auto prepare_renderer_per_camera_data(gfx::Camera const& camera) -> void;
    auto render_camera(gfx::Camera const& camera, gfx::RenderGraph& rg) -> void;
};

}
