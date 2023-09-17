#pragma once

#include "../prelude/idiom.hpp"
#include "../graphics/camera.hpp"
#include "../graphics/render_graph.hpp"

namespace bi {

struct ForwardRenderer final : PImpl<ForwardRenderer> {
    struct Impl;

    ForwardRenderer();
    ~ForwardRenderer();

    ForwardRenderer(ForwardRenderer&& rhs) noexcept;
    auto operator=(ForwardRenderer&& rhs) noexcept -> ForwardRenderer&;

    static constexpr std::string_view renderer_type_name = "ForwardRenderer";

    auto prepare_renderer_per_frame_data() -> void;
    auto prepare_renderer_per_camera_data(gfx::Camera const& camera) -> void;
    auto render_camera(gfx::Camera const& camera, gfx::RenderGraph& rg) -> void;
};

}
