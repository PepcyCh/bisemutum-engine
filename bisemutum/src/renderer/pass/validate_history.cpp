#include "validate_history.hpp"

#include <bisemutum/prelude/math.hpp>
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/window/window.hpp>

namespace bi {

namespace {

BI_SHADER_PARAMETERS_BEGIN(ValidateHistoryPassParams)
    BI_SHADER_PARAMETER(uint, has_history)
    BI_SHADER_PARAMETER(float, _pad)
    BI_SHADER_PARAMETER(uint2, tex_size)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, velocity_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, depth_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, gbuffer_normal)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, hist_depth_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, hist_gbuffer_normal)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<uint>, out_validation_tex, rhi::ResourceFormat::r8_uint)
BI_SHADER_PARAMETERS_END()

struct PassData final {
    gfx::TextureHandle velocity_tex;
    gfx::TextureHandle depth_tex;
    gfx::TextureHandle gbuffer_normal;
    gfx::TextureHandle hist_depth_tex;
    gfx::TextureHandle hist_gbuffer_normal;
    gfx::TextureHandle out_validation_tex;
};

}

ValidateHistoryPass::ValidateHistoryPass() {
    compute_shader_params_.initialize<ValidateHistoryPassParams>();

    compute_shader_.source.path = "/bisemutum/shaders/renderer/validate_history.hlsl";
    compute_shader_.source.entry = "validate_history_cs";
    compute_shader_.set_shader_params_struct<ValidateHistoryPassParams>();
}

auto ValidateHistoryPass::render(
    gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input
) -> gfx::TextureHandle {
    auto width = camera.target_texture().desc().extent.width;
    auto height = camera.target_texture().desc().extent.height;

    auto frame_count = g_engine->window()->frame_count();
    auto [frame_count_it, is_new_camera] = last_frame_counts_.try_emplace(&camera);
    auto has_history = !is_new_camera && frame_count_it->second + 1 == frame_count;
    frame_count_it->second = frame_count;

    auto [builder, pass_data] = rg.add_compute_pass<PassData>("Validate History");

    pass_data->velocity_tex = builder.read(input.velocity);
    pass_data->depth_tex = builder.read(input.depth);
    pass_data->gbuffer_normal = builder.read(input.normal_roughness);
    auto hist_depth = camera.get_history_texture("depth");
    auto hist_gbuffer_normal = camera.get_history_texture("gbuffer_normal_roughness");
    has_history &= hist_depth != gfx::TextureHandle::invalid && hist_gbuffer_normal != gfx::TextureHandle::invalid;
    if (has_history) {
        pass_data->hist_depth_tex = builder.read(hist_depth);
        pass_data->hist_gbuffer_normal = builder.read(hist_gbuffer_normal);
    }

    auto validation_tex = rg.add_texture([width, height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(rhi::ResourceFormat::r8_uint, width, height)
            // TODO - add utexture and itexture in shader_param.hpp
            // .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write});
            .usage({rhi::TextureUsage::storage_read_write});
    });
    pass_data->out_validation_tex = builder.write(validation_tex);

    builder.set_execution_function<PassData>(
        [this, &camera, width, height, has_history](CRef<PassData> pass_data, gfx::ComputePassContext const& ctx) {
            auto params = compute_shader_params_.mutable_typed_data<ValidateHistoryPassParams>();
            params->has_history = has_history;
            params->tex_size = {width, height};
            params->velocity_tex = {ctx.rg->texture(pass_data->velocity_tex)};
            params->depth_tex = {ctx.rg->texture(pass_data->depth_tex)};
            params->gbuffer_normal = {ctx.rg->texture(pass_data->gbuffer_normal)};
            if (has_history) {
                params->hist_depth_tex = {ctx.rg->texture(pass_data->hist_depth_tex)};
                params->hist_gbuffer_normal = {ctx.rg->texture(pass_data->hist_gbuffer_normal)};
            } else {
                params->hist_depth_tex = {nullptr};
                params->hist_gbuffer_normal = {nullptr};
            }
            params->out_validation_tex = {ctx.rg->texture(pass_data->out_validation_tex)};
            compute_shader_params_.update_uniform_buffer();
            ctx.dispatch(
                camera, compute_shader_, compute_shader_params_,
                ceil_div(width, 16u), ceil_div(height, 16u)
            );
            camera.add_history_texture("depth", pass_data->depth_tex);
            camera.add_history_texture("gbuffer_normal_roughness", pass_data->gbuffer_normal);
        }
    );

    return validation_tex;
}

}
