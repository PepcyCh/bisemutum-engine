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
        path_tracing,
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
        bool half_resolution = true;
    };

    struct ReflectionSettings final {
        enum class Mode : uint8_t {
            none,
            screen_space,
            raytraced,
        };
        Mode mode = Mode::none;
        float range = 16.0f;
        float strength = 1.0f;
        float max_roughness = 0.3f;
        float fade_roughness = 0.1f;
        bool denoise = true;
        bool half_resolution = true;
    };

    struct IndirectDiffuseSettings final {
        enum class Mode : uint8_t {
            none,
            ddgi,
        };
        Mode mode = Mode::none;
        float strength = 1.0f;
    };

    struct PathTracingSettings final {
        float ray_length = 100.0f;
        uint32_t max_bounces = 3;
        bool denoise = true;
        bool accumulate = true;
    };

    struct Settings final {
        PipelineMode pipeline_mode = PipelineMode::deferred;
        ShadowSettings shadow;
        AmbientOcclusionSettings ambient_occlusion;
        ReflectionSettings reflection;
        IndirectDiffuseSettings indirect_diffuse;
        PathTracingSettings path_tracing;
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
    field(half_resolution),
)
BI_SREFL(
    type(BasicRenderer::ReflectionSettings),
    field(mode),
    field(range, RangeF{}),
    field(strength, RangeF{0.0f, 1.0f}),
    field(max_roughness, RangeF{0.0f, 1.0f}),
    field(fade_roughness, RangeF{0.0f, 1.0f}),
    field(denoise),
    field(half_resolution),
)
BI_SREFL(
    type(BasicRenderer::IndirectDiffuseSettings),
    field(mode),
    field(strength, RangeF{0.0f, 1.0f}),
)
BI_SREFL(
    type(BasicRenderer::PathTracingSettings),
    field(ray_length, RangeF{}),
    field(max_bounces, RangeI{2, 16}),
    field(denoise),
    field(accumulate),
)
BI_SREFL(
    type(BasicRenderer::Settings),
    field(pipeline_mode),
    field(shadow),
    field(ambient_occlusion),
    field(reflection),
    field(indirect_diffuse),
    field(path_tracing),
)

struct BasicRendererOverrideVolume final {
    static constexpr std::string_view component_type_name = "BasicRendererOverrideVolume";

    // For volume componnet
    static constexpr bool global{true};
    float priority{0.0f};

    BasicRenderer::Settings settings;
};
BI_SREFL(
    type(BasicRendererOverrideVolume),
    field(priority, RangeF{}),
    field(settings),
)

}
