#include <bisemutum/renderer/basic.hpp>

#include <bisemutum/runtime/component_utils.hpp>
#include <bisemutum/runtime/system_manager.hpp>
#include <bisemutum/graphics/gpu_scene_system.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>

#include "context/lights.hpp"
#include "context/skybox.hpp"
#include "pass/skybox_precompute.hpp"
#include "pass/skybox.hpp"
#include "pass/shadow_mapping.hpp"
#include "pass/forward.hpp"
#include "pass/gbuffer.hpp"
#include "pass/deferred_lighting.hpp"
#include "pass/validate_history.hpp"
#include "pass/ambient_occlusion.hpp"
#include "pass/reflection.hpp"
#include "pass/post_process.hpp"

namespace bi {

struct BasicRenderer::Impl final {
    auto prepare_renderer_per_frame_data() -> void {
        auto& settings = rt::find_volume_component_for(float3(0.0f), default_settings).settings;

        lights_ctx.dir_light_shadow_map_resolution = 1u << static_cast<uint32_t>(settings.shadow.dir_light_resolution);
        lights_ctx.point_light_shadow_map_resolution = 1u << static_cast<uint32_t>(settings.shadow.point_light_resolution);
        lights_ctx.collect_all_lights();

        lights_ctx.update_shader_params();
        skybox_ctx.update_shader_params();

        forward_pass.update_params(lights_ctx, skybox_ctx);
        deferred_lighting_pass.update_params(lights_ctx, skybox_ctx);
        skybox_pass.update_params(skybox_ctx);

        if (
            settings.reflection.mode != ReflectionSettings::Mode::none
            && settings.pipeline_mode == PipelineMode::deferred
        ) {
            reflection_pass.update_params(lights_ctx, skybox_ctx);
        }
    }

    auto prepare_renderer_per_camera_data(gfx::Camera const& camera) -> void {
        lights_ctx.prepare_dir_lights_per_camera(camera);
    }

    auto render_camera(gfx::Camera const& camera, gfx::RenderGraph& rg) -> void {
        auto& settings = rt::find_volume_component_for(camera.position, default_settings).settings;

        auto drawables = g_engine->system_manager()->get_system_for_current_scene<gfx::GpuSceneSystem>()->get_all_drawables();

        if (g_engine->graphics_manager()->device()->properties().raytracing_pipeline) {
            gfx::AccelerationStructureDesc accel_desc{drawables};
            scene_accel = rg.add_acceleration_structure(accel_desc);
        }

        auto skybox = skybox_precompute_pass.render(rg, {
            .skybox_ctx = skybox_ctx,
        });

        auto shadow_maps = shadow_mapping_pass.render(camera, rg, {
            .drawables = drawables,
            .lights_ctx = lights_ctx,
        });

        gfx::TextureHandle color;
        gfx::TextureHandle depth;
        gfx::TextureHandle velocity;
        GBufferTextures gbuffer;
        if (settings.pipeline_mode == PipelineMode::forward) {
            auto forward_output_data = forward_pass.render(camera, rg, {
                .drawables = drawables,
                .shadow_maps = shadow_maps,
                .skybox = skybox,
            });
            color = forward_output_data.output;
            depth = forward_output_data.depth;
            velocity = forward_output_data.velocity;
        } else {
            auto gbuffer_output = gbuffer_pass.render(camera, rg, drawables);
            gbuffer = gbuffer_output.gbuffer;
            auto lighting_output = deferred_lighting_pass.render(camera, rg, {
                .color = gbuffer_output.color,
                .depth = gbuffer_output.depth,
                .gbuffer = gbuffer_output.gbuffer,
                .shadow_maps = shadow_maps,
                .skybox = skybox,
            });
            color = lighting_output.output;
            depth = gbuffer_output.depth;
            velocity = gbuffer_output.velocity;
        }
        auto skybox_output = skybox_pass.render(camera, rg, {
            .color = color,
            .depth = depth,
            .skybox = skybox,
        });
        color = skybox_output.color;
        depth = skybox_output.depth;

        if (settings.pipeline_mode == PipelineMode::deferred) {
            auto history_validation = validate_history_pass.render(camera, rg, {
                .velocity = velocity,
                .depth = depth,
                .normal_roughness = gbuffer.normal_roughness,
            });

            if (settings.ambient_occlusion.mode != AmbientOcclusionSettings::Mode::none) {
                color = ambient_occlusion_pass.render(camera, rg, {
                    .color = color,
                    .depth = depth,
                    .normal_roughness = gbuffer.normal_roughness,
                    .velocity = velocity,
                    .history_validation = history_validation,
                    .scene_accel = scene_accel,
                }, settings.ambient_occlusion);
            }

            if (settings.reflection.mode != ReflectionSettings::Mode::none) {
                color = reflection_pass.render(camera, rg, {
                    .color = color,
                    .velocity = velocity,
                    .depth = depth,
                    .gbuffer = gbuffer,
                    .history_validation = history_validation,
                    .shadow_maps = shadow_maps,
                    .skybox = skybox,
                    .scene_accel = scene_accel,
                }, settings.reflection);
            }
        }

        post_process_pass.render(camera, rg, {
            .color = color,
            .depth = depth,
        });
    }

    inline static BasicRendererOverrideVolume default_settings;

    LightsContext lights_ctx;
    SkyboxContext skybox_ctx;
    gfx::AccelerationStructureHandle scene_accel = gfx::AccelerationStructureHandle::invalid;

    SkyboxPrecomputePass skybox_precompute_pass;
    ShadowMappingPass shadow_mapping_pass;

    ForwardPass forward_pass;
    GBufferdPass gbuffer_pass;
    DeferredLightingPass deferred_lighting_pass;
    SkyboxPass skybox_pass;

    ValidateHistoryPass validate_history_pass;

    AmbientOcclusionPass ambient_occlusion_pass;
    ReflectionPass reflection_pass;
    PostProcessPass post_process_pass;
};

BasicRenderer::BasicRenderer() = default;
BasicRenderer::~BasicRenderer() = default;

BasicRenderer::BasicRenderer(BasicRenderer&& rhs) noexcept = default;
auto BasicRenderer::operator=(BasicRenderer&& rhs) noexcept -> BasicRenderer& = default;

auto BasicRenderer::override_volume_component_name() const -> std::string_view {
    return BasicRendererOverrideVolume::component_type_name;
}

auto BasicRenderer::prepare_renderer_per_frame_data() -> void {
    impl()->prepare_renderer_per_frame_data();
}
auto BasicRenderer::prepare_renderer_per_camera_data(gfx::Camera const& camera) -> void {
    impl()->prepare_renderer_per_camera_data(camera);
}
auto BasicRenderer::render_camera(gfx::Camera const& camera, gfx::RenderGraph& rg) -> void {
    impl()->render_camera(camera, rg);
}

}
