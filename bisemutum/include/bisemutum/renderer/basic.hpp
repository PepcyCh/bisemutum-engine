#pragma once

#include "../prelude/idiom.hpp"
#include "../graphics/camera.hpp"
#include "../graphics/render_graph.hpp"

namespace bi {

struct BasicRenderer final : PImpl<BasicRenderer> {
    struct Impl;

    enum class PipelineMode : uint8_t {
        forward,
        deferred,
    };

    enum class ShadowMapResolution : uint8_t {
        _128 = 7,
        _256,
        _512,
        _1024,
        _2048,
        _4096,
    };

    struct ShadowSettings final {
        enum class Resolution : uint8_t {
            _128 = 7,
            _256,
            _512,
            _1024,
            _2048,
            _4096,
        };
        Resolution dir_light_resolution = Resolution::_2048;
        Resolution point_light_resolution = Resolution::_512;
    };

    struct AmbientOcclusionSettings final {
        enum class Mode : uint8_t {
            none,
            screen_space,
            raytraced,
        };
        Mode mode = Mode::none;
        float range = 0.5f;
        float strength = 0.5f;
    };

    struct Settings final {
        PipelineMode pipeline_mode = PipelineMode::deferred;
        ShadowSettings shadow;
        AmbientOcclusionSettings ambient_occlusion;
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
    type(BasicRenderer::ShadowSettings),
    field(dir_light_resolution),
    field(point_light_resolution),
)
BI_SREFL(
    type(BasicRenderer::AmbientOcclusionSettings),
    field(mode),
    field(range, RangeF{}),
    field(strength, RangeF{0.0f, 1.0f}),
)
BI_SREFL(
    type(BasicRenderer::Settings),
    field(pipeline_mode),
    field(shadow),
    field(ambient_occlusion),
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
