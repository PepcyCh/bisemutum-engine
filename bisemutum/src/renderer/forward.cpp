#include <bisemutum/renderer/forward.hpp>

#include "context/lights.hpp"
#include "pass/shadow_mapping.hpp"
#include "pass/forward.hpp"
#include "pass/post_process.hpp"

namespace bi {

struct ForwardRenderer::Impl final {
    auto prepare_renderer_per_frame_data() -> void {
        lights_ctx.collect_all_lights();
        forward_pass.update_params(lights_ctx);
    }

    auto prepare_renderer_per_camera_data(gfx::Camera const& camera) -> void {}

    auto render_camera(gfx::Camera const& camera, gfx::RenderGraph& rg) -> void {
        auto dir_lighst_shadow_map = shadow_mapping_pass.render(camera, rg, {
            .lights_ctx = lights_ctx,
        });

        auto forward_pass_data = forward_pass.render(camera, rg, {
            .dir_lighst_shadow_map = dir_lighst_shadow_map,
        });

        post_process_pass.render(camera, rg, {
            .color = forward_pass_data->output,
            .depth = forward_pass_data->depth,
        });
    }

    LightsContext lights_ctx;

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
