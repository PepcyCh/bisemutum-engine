#pragma once

#include "../prelude/idiom.hpp"
#include "../graphics/camera.hpp"
#include "../graphics/render_graph.hpp"

namespace bi {

struct BasicRenderer final : PImpl<BasicRenderer> {
    struct Impl;

    enum class Mode {
        forward,
        deferred,
    };

    struct Settings final {
        Mode mode = Mode::deferred;
    };

    BasicRenderer();
    ~BasicRenderer();

    BasicRenderer(BasicRenderer&& rhs) noexcept;
    auto operator=(BasicRenderer&& rhs) noexcept -> BasicRenderer&;

    static constexpr std::string_view renderer_type_name = "BasicRenderer";

    auto override_volume_component_name() const -> std::string_view;
    auto prepare_renderer_per_frame_data() -> void;
    auto prepare_renderer_per_camera_data(gfx::Camera const& camera) -> void;
    auto render_camera(gfx::Camera const& camera, gfx::RenderGraph& rg) -> void;
};

BI_SREFL(
    type(BasicRenderer::Settings),
    field(mode),
)

struct BasicRendererOverrideVolume final {
    static constexpr std::string_view component_type_name = "BasicRendererOverrideVolume";

    bool global{true};
    float priority{0.0f};

    BasicRenderer::Settings settings;
};
BI_SREFL(
    type(BasicRendererOverrideVolume),
    field(global),
    field(priority, RangeF{}),
    field(settings),
)

}
