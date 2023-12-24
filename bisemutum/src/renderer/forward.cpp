#include <bisemutum/renderer/forward.hpp>

#include "context/lights.hpp"
#include "context/skybox.hpp"
#include "pass/skybox_precompute.hpp"
#include "pass/shadow_mapping.hpp"
#include "pass/forward.hpp"
#include "pass/post_process.hpp"

namespace bi {

struct ForwardRenderer::Impl final {
    auto prepare_renderer_per_frame_data() -> void {
        lights_ctx.collect_all_lights();

        forward_pass.update_params(lights_ctx, skybox_ctx);
    }

    auto prepare_renderer_per_camera_data(gfx::Camera const& camera) -> void {
        lights_ctx.prepare_dir_lights_per_camera(camera);
    }

    auto render_camera(gfx::Camera const& camera, gfx::RenderGraph& rg) -> void {
        auto skybox = skybox_precompute_pass.render(rg, {
            .skybox_ctx = skybox_ctx,
        });

        auto dir_lighst_shadow_map = shadow_mapping_pass.render(camera, rg, {
            .lights_ctx = lights_ctx,
        });

        auto forward_output_data = forward_pass.render(camera, rg, {
            .dir_lighst_shadow_map = dir_lighst_shadow_map,
            .skybox = skybox,
        });

        post_process_pass.render(camera, rg, {
            .color = forward_output_data.output,
            .depth = forward_output_data.depth,
        });
    }

    LightsContext lights_ctx;
    SkyboxContext skybox_ctx;

    SkyboxPrecomputePass skybox_precompute_pass;
    ShadowMappingPass shadow_mapping_pass;
    ForwardPass forward_pass;
    PostProcessPass post_process_pass;
};

ForwardRenderer::ForwardRenderer() = default;
ForwardRenderer::~ForwardRenderer() = default;

ForwardRenderer::ForwardRenderer(ForwardRenderer&& rhs) noexcept = default;
auto ForwardRenderer::operator=(ForwardRenderer&& rhs) noexcept -> ForwardRenderer& = default;

auto ForwardRenderer::prepare_renderer_per_frame_data() -> void {
    impl()->prepare_renderer_per_frame_data();
}
auto ForwardRenderer::prepare_renderer_per_camera_data(gfx::Camera const& camera) -> void {
    impl()->prepare_renderer_per_camera_data(camera);
}
auto ForwardRenderer::render_camera(gfx::Camera const& camera, gfx::RenderGraph& rg) -> void {
    impl()->render_camera(camera, rg);
}

}
