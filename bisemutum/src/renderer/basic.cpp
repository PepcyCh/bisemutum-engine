#include <bisemutum/renderer/basic.hpp>

#include <bisemutum/runtime/component_utils.hpp>

#include "context/lights.hpp"
#include "context/skybox.hpp"
#include "pass/skybox_precompute.hpp"
#include "pass/shadow_mapping.hpp"
#include "pass/forward.hpp"
#include "pass/gbuffer.hpp"
#include "pass/deferred_lighting.hpp"
#include "pass/post_process.hpp"

namespace bi {

struct BasicRenderer::Impl final {
    auto prepare_renderer_per_frame_data() -> void {
        lights_ctx.collect_all_lights();

        forward_pass.update_params(lights_ctx, skybox_ctx);
        deferred_lighting_pass.update_params(lights_ctx, skybox_ctx);
    }

    auto prepare_renderer_per_camera_data(gfx::Camera const& camera) -> void {
        lights_ctx.prepare_dir_lights_per_camera(camera);
    }

    auto render_camera(gfx::Camera const& camera, gfx::RenderGraph& rg) -> void {
        auto& settings = rt::find_volume_component_for(camera.position, default_settings).settings;

        auto skybox = skybox_precompute_pass.render(rg, {
            .skybox_ctx = skybox_ctx,
        });

        auto dir_lighst_shadow_map = shadow_mapping_pass.render(camera, rg, {
            .lights_ctx = lights_ctx,
        });

        gfx::TextureHandle color;
        gfx::TextureHandle depth;
        if (settings.mode == Mode::forward) {
            auto forward_output_data = forward_pass.render(camera, rg, {
                .dir_lighst_shadow_map = dir_lighst_shadow_map,
                .skybox = skybox,
            });
            color = forward_output_data.output;
            depth = forward_output_data.depth;
        } else {
            auto gbuffer_output = gbuffer_pass.render(camera, rg);
            auto lighting_output = deferred_lighting_pass.render(camera, rg, {
                .depth = gbuffer_output.depth,
                .gbuffer = gbuffer_output.gbuffer,
                .dir_lighst_shadow_map = dir_lighst_shadow_map,
                .skybox = skybox,
            });
            color = lighting_output.output;
            depth = gbuffer_output.depth;
        }

        post_process_pass.render(camera, rg, {
            .color = color,
            .depth = depth,
        });
    }

    inline static BasicRendererOverrideVolume default_settings;

    LightsContext lights_ctx;
    SkyboxContext skybox_ctx;

    SkyboxPrecomputePass skybox_precompute_pass;
    ShadowMappingPass shadow_mapping_pass;

    ForwardPass forward_pass;
    GBufferdPass gbuffer_pass;
    DeferredLightingPass deferred_lighting_pass;

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
