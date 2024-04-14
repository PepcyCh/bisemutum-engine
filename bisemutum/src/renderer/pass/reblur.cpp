#include "reblur.hpp"

#include <bisemutum/prelude/math.hpp>
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/window/window.hpp>

namespace bi {

namespace {

BI_SHADER_PARAMETERS_BEGIN(PreBlurPassParams)
    BI_SHADER_PARAMETER(uint2, tex_size)
    BI_SHADER_PARAMETER(uint2, gbuffer_tex_size)
    BI_SHADER_PARAMETER(float4, poisson_rotator)
    BI_SHADER_PARAMETER(float, in_blur_radius)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, in_color)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, in_dist)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, depth_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, normal_roughness_tex)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, out_lighting_dist_tex, rhi::ResourceFormat::rgba16_sfloat)
BI_SHADER_PARAMETERS_END()

struct PreBlurPassData final {
    gfx::TextureHandle in_color;
    gfx::TextureHandle in_dist;
    gfx::TextureHandle depth_tex;
    gfx::TextureHandle normal_roughness_tex;
    gfx::TextureHandle out_lighting_dist_tex;
};

BI_SHADER_PARAMETERS_BEGIN(TemporalAccumulatePassParams)
    BI_SHADER_PARAMETER(uint2, tex_size)
    BI_SHADER_PARAMETER(uint2, gbuffer_tex_size)
    BI_SHADER_PARAMETER(uint, virtual_history)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, in_lighting_dist_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, depth_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, normal_roughness_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, velocity_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, hist_depth_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, hist_normal_roughness_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, hist_lighting_dist_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, hist_accumulation_tex)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<uint>, history_validation_tex, rhi::ResourceFormat::r8_uint)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, out_lighting_dist_tex, rhi::ResourceFormat::rgba16_sfloat)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, accumulation_tex, rhi::ResourceFormat::r16_sfloat)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, input_sampler)
BI_SHADER_PARAMETERS_END()

struct TemporalAccumulatePassData final {
    gfx::TextureHandle in_lighting_dist_tex;
    gfx::TextureHandle depth_tex;
    gfx::TextureHandle normal_roughness_tex;
    gfx::TextureHandle velocity_tex;
    gfx::TextureHandle hist_depth_tex;
    gfx::TextureHandle hist_normal_roughness_tex;
    gfx::TextureHandle hist_lighting_dist_tex;
    gfx::TextureHandle hist_accumulation_tex;
    gfx::TextureHandle history_validation_tex;
    gfx::TextureHandle out_lighting_dist_tex;
    gfx::TextureHandle accumulation_tex;
};

BI_SHADER_PARAMETERS_BEGIN(FetchLinearDepthPassParams)
    BI_SHADER_PARAMETER(uint2, tex_size)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, in_tex)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float>, out_tex, rhi::ResourceFormat::r32_sfloat)
BI_SHADER_PARAMETERS_END()

struct FetchLinearDepthPassData final {
    gfx::TextureHandle in_tex;
    gfx::TextureHandle out_tex;
};

BI_SHADER_PARAMETERS_BEGIN(GenDepthMipPassParams)
    BI_SHADER_PARAMETER(uint2, tex_size)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, in_tex, rhi::ResourceFormat::rgba16_sfloat)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float>, in_depth_tex, rhi::ResourceFormat::r32_sfloat)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, out_tex_0, rhi::ResourceFormat::rgba16_sfloat)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float>, out_depth_tex_0, rhi::ResourceFormat::r32_sfloat)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, out_tex_1, rhi::ResourceFormat::rgba16_sfloat)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float>, out_depth_tex_1, rhi::ResourceFormat::r32_sfloat)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, out_tex_2, rhi::ResourceFormat::rgba16_sfloat)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float>, out_depth_tex_2, rhi::ResourceFormat::r32_sfloat)
BI_SHADER_PARAMETERS_END()

struct GenDepthMipPassData final {
    gfx::TextureHandle color_tex;
    gfx::TextureHandle depth_tex;
};

BI_SHADER_PARAMETERS_BEGIN(FixHistoryPassParams)
    BI_SHADER_PARAMETER(uint2, tex_size)
    BI_SHADER_PARAMETER(uint2, gbuffer_tex_size)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, in_lighting_dist_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, accumulation_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, depth_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, normal_roughness_tex)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, out_lighting_dist_tex, rhi::ResourceFormat::rgba16_sfloat)
BI_SHADER_PARAMETERS_END()

struct FixHistoryPassData final {
    gfx::TextureHandle in_lighting_dist_tex;
    gfx::TextureHandle accumulation_tex;
    gfx::TextureHandle depth_tex;
    gfx::TextureHandle normal_roughness_tex;
    gfx::TextureHandle out_lighting_dist_tex;
};

BI_SHADER_PARAMETERS_BEGIN(BlurPassParams)
    BI_SHADER_PARAMETER(uint2, tex_size)
    BI_SHADER_PARAMETER(uint2, gbuffer_tex_size)
    BI_SHADER_PARAMETER(float4, poisson_rotator)
    BI_SHADER_PARAMETER(float, in_blur_radius)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, in_lighting_dist_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, accumulation_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, depth_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, normal_roughness_tex)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, out_lighting_dist_tex, rhi::ResourceFormat::rgba16_sfloat)
BI_SHADER_PARAMETERS_END()

struct BlurPassData final {
    gfx::TextureHandle in_lighting_dist_tex;
    gfx::TextureHandle accumulation_tex;
    gfx::TextureHandle depth_tex;
    gfx::TextureHandle normal_roughness_tex;
    gfx::TextureHandle out_lighting_dist_tex;
};

BI_SHADER_PARAMETERS_BEGIN(TemporalStabilizePassParams)
    BI_SHADER_PARAMETER(uint2, tex_size)
    BI_SHADER_PARAMETER(uint2, gbuffer_tex_size)
    BI_SHADER_PARAMETER(float, anti_flickering_strength)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, in_lighting_dist_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, depth_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, normal_roughness_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, velocity_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, hist_lighting_dist_tex)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<uint>, history_validation_tex, rhi::ResourceFormat::r8_uint)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, out_lighting_dist_tex, rhi::ResourceFormat::rgba16_sfloat)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, input_sampler)
BI_SHADER_PARAMETERS_END()

struct TemporalStabilizePassData final {
    gfx::TextureHandle in_lighting_dist_tex;
    gfx::TextureHandle depth_tex;
    gfx::TextureHandle normal_roughness_tex;
    gfx::TextureHandle velocity_tex;
    gfx::TextureHandle hist_lighting_dist_tex;
    gfx::TextureHandle history_validation_tex;
    gfx::TextureHandle out_lighting_dist_tex;
};

BI_SHADER_PARAMETERS_BEGIN(PostBlurPassParams)
    BI_SHADER_PARAMETER(uint2, tex_size)
    BI_SHADER_PARAMETER(uint2, gbuffer_tex_size)
    BI_SHADER_PARAMETER(float4, poisson_rotator)
    BI_SHADER_PARAMETER(float, in_blur_radius)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, in_lighting_dist_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, accumulation_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, depth_tex)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, normal_roughness_tex)
    BI_SHADER_PARAMETER_UAV_TEXTURE(RWTexture2D<float4>, out_lighting_dist_tex, rhi::ResourceFormat::rgba16_sfloat)
BI_SHADER_PARAMETERS_END()

struct PostBlurPassData final {
    gfx::TextureHandle in_lighting_dist_tex;
    gfx::TextureHandle accumulation_tex;
    gfx::TextureHandle depth_tex;
    gfx::TextureHandle normal_roughness_tex;
    gfx::TextureHandle out_lighting_dist_tex;
};

constexpr std::array<float, 32> pre_blur_rotator_rands{
    0.840188f, 0.394383f, 0.783099f, 0.79844f, 0.911647f, 0.197551f, 0.335223f, 0.76823f,
    0.277775f, 0.55397f, 0.477397f, 0.628871f, 0.364784f, 0.513401f, 0.95223f, 0.916195f,
    0.635712f, 0.717297f, 0.141603f, 0.606969f, 0.0163006f, 0.242887f, 0.137232f, 0.804177f,
    0.156679f, 0.400944f, 0.12979f, 0.108809f, 0.998924f, 0.218257f, 0.512932f, 0.839112f,
};
constexpr std::array<float, 32> blur_rotator_rands{
    0.61264f, 0.296032f, 0.637552f, 0.524287f, 0.493583f, 0.972775f, 0.292517f, 0.771358f,
    0.526745f, 0.769914f, 0.400229f, 0.891529f, 0.283315f, 0.352458f, 0.807725f, 0.919026f,
    0.0697553f, 0.949327f, 0.525995f, 0.0860558f, 0.192214f, 0.663227f, 0.890233f, 0.348893f,
    0.0641713f, 0.020023f, 0.457702f, 0.0630958f, 0.23828f, 0.970634f, 0.902208f, 0.85092f,
};
constexpr std::array<float, 32> post_blur_rotator_rands{
    0.266666f, 0.53976f, 0.375207f, 0.760249f, 0.512535f, 0.667724f, 0.531606f, 0.0392803f,
    0.437638f, 0.931835f, 0.93081f, 0.720952f, 0.284293f, 0.738534f, 0.639979f, 0.354049f,
    0.687861f, 0.165974f, 0.440105f, 0.880075f, 0.829201f, 0.330337f, 0.228968f, 0.893372f,
    0.35036f, 0.68667f, 0.956468f, 0.58864f, 0.657304f, 0.858676f, 0.43956f, 0.92397f,
};

auto get_rotator(float angle) -> float4 {
    auto ca = std::cos(angle);
    auto sa = std::sin(angle);
    return float4(ca, sa, -sa, ca);
};

} // namespace

ReblurPass::ReblurPass() {
    pre_blur_shader_params_.initialize<PreBlurPassParams>();
    pre_blur_shader_.source.path = "/bisemutum/shaders/renderer/reblur/pre_blur.hlsl";
    pre_blur_shader_.source.entry = "pre_blur_cs";
    pre_blur_shader_.set_shader_params_struct<PreBlurPassParams>();

    temporal_accumulate_shader_params_.initialize<TemporalAccumulatePassParams>();
    temporal_accumulate_shader_.source.path = "/bisemutum/shaders/renderer/reblur/temporal_accumulate.hlsl";
    temporal_accumulate_shader_.source.entry = "temporal_accumulate_cs";
    temporal_accumulate_shader_.set_shader_params_struct<TemporalAccumulatePassParams>();

    fetch_linear_depth_shader_params_.initialize<FetchLinearDepthPassParams>();
    fetch_linear_depth_shader_.source.path = "/bisemutum/shaders/renderer/reblur/fetch_linear_depth.hlsl";
    fetch_linear_depth_shader_.source.entry = "fetch_linear_depth_cs";
    fetch_linear_depth_shader_.set_shader_params_struct<FetchLinearDepthPassParams>();

    gen_depth_mip_shader_params_.initialize<GenDepthMipPassParams>();
    gen_depth_mip_shader_.source.path = "/bisemutum/shaders/renderer/reblur/gen_depth_mip.hlsl";
    gen_depth_mip_shader_.source.entry = "gen_depth_mip_cs";
    gen_depth_mip_shader_.set_shader_params_struct<GenDepthMipPassParams>();

    fix_history_shader_params_.initialize<FixHistoryPassParams>();
    fix_history_shader_.source.path = "/bisemutum/shaders/renderer/reblur/fix_history.hlsl";
    fix_history_shader_.source.entry = "fix_history_cs";
    fix_history_shader_.set_shader_params_struct<FixHistoryPassParams>();

    blur_shader_params_.initialize<BlurPassParams>();
    blur_shader_.source.path = "/bisemutum/shaders/renderer/reblur/blur.hlsl";
    blur_shader_.source.entry = "blur_cs";
    blur_shader_.set_shader_params_struct<BlurPassParams>();

    temporal_stabilize_shader_params_.initialize<TemporalStabilizePassParams>();
    temporal_stabilize_shader_.source.path = "/bisemutum/shaders/renderer/reblur/temporal_stabilize.hlsl";
    temporal_stabilize_shader_.source.entry = "temporal_stabilize_cs";
    temporal_stabilize_shader_.set_shader_params_struct<TemporalStabilizePassParams>();

    post_blur_shader_params_.initialize<PostBlurPassParams>();
    post_blur_shader_.source.path = "/bisemutum/shaders/renderer/reblur/post_blur.hlsl";
    post_blur_shader_.source.entry = "post_blur_cs";
    post_blur_shader_.set_shader_params_struct<PostBlurPassParams>();

    sampler_ = g_engine->graphics_manager()->get_sampler(rhi::SamplerDesc{
        .mag_filter = rhi::SamplerFilterMode::linear,
        .min_filter = rhi::SamplerFilterMode::linear,
        .address_mode_u = rhi::SamplerAddressMode::clamp_to_edge,
        .address_mode_v = rhi::SamplerAddressMode::clamp_to_edge,
        .address_mode_w = rhi::SamplerAddressMode::clamp_to_edge,
    });
}

auto ReblurPass::render(gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input) -> gfx::TextureHandle {
    auto width = camera.target_texture().desc().extent.width;
    auto height = camera.target_texture().desc().extent.height;

    auto frame_count = g_engine->window()->frame_count();
    auto [frame_count_it, is_new_camera] = last_frame_counts_.try_emplace(&camera);
    auto has_history = !is_new_camera && frame_count_it->second + 1 == frame_count;
    frame_count_it->second = frame_count;

    auto denoised = rg.add_texture([width, height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(rhi::ResourceFormat::rgba16_sfloat, width, height)
            .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write});
    });

    auto lighting_dist_0 = rg.add_texture([width, height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(rhi::ResourceFormat::rgba16_sfloat, width, height)
            .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write})
            .mipmap(4);
    });
    auto lighting_dist_1 = rg.add_texture([width, height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(rhi::ResourceFormat::rgba16_sfloat, width, height)
            .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write});
    });

    auto accumulation = rg.add_texture([width, height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(rhi::ResourceFormat::r16_sfloat, width, height)
            .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write});
    });

    auto linear_depth = rg.add_texture([width, height](gfx::TextureBuilder& builder) {
        builder
            .dim_2d(rhi::ResourceFormat::r32_sfloat, width, height)
            .usage({rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write})
            .mipmap(4);
    });

    static constexpr auto blur_radius = 0.9f;
    static constexpr auto anti_flickering_strength = 3.5f;
    static constexpr auto reblur_tile_size = 8u;

    const auto rotator_index = g_engine->window()->frame_count() % blur_rotator_rands.size();

    {
        auto [builder, pass_data] = rg.add_compute_pass<PreBlurPassData>("ReBLUR Pre Blur");
        pass_data->in_color = builder.read(input.noised_tex);
        pass_data->in_dist = builder.read(input.hit_positions_tex);
        pass_data->depth_tex = builder.read(input.depth);
        pass_data->normal_roughness_tex = builder.read(input.gbuffer.normal_roughness);
        pass_data->out_lighting_dist_tex = builder.write(lighting_dist_1);
        builder.set_execution_function<PreBlurPassData>(
            [this, &camera, width, height, rotator_index](CRef<PreBlurPassData> pass_data, gfx::ComputePassContext const& ctx) {
                auto params = pre_blur_shader_params_.mutable_typed_data<PreBlurPassParams>();
                params->tex_size = {width, height};
                params->gbuffer_tex_size = {width, height};
                params->in_blur_radius = blur_radius;
                params->poisson_rotator = get_rotator(pre_blur_rotator_rands[rotator_index]);
                params->in_color = {ctx.rg->texture(pass_data->in_color)};
                params->in_dist = {ctx.rg->texture(pass_data->in_dist)};
                params->depth_tex = {ctx.rg->texture(pass_data->depth_tex)};
                params->normal_roughness_tex = {ctx.rg->texture(pass_data->normal_roughness_tex)};
                params->out_lighting_dist_tex = {ctx.rg->texture(pass_data->out_lighting_dist_tex)};
                pre_blur_shader_params_.update_uniform_buffer();
                ctx.dispatch(
                    camera, pre_blur_shader_, pre_blur_shader_params_,
                    ceil_div(width, reblur_tile_size), ceil_div(height, reblur_tile_size)
                );
            }
        );
    }

    auto hist_depth = camera.get_history_texture("depth");
    auto hist_normal_roughness = camera.get_history_texture("gbuffer_normal_roughness");
    auto hist_lighting_dist_0 = camera.get_history_texture("reblur_lighting_dist_0");
    auto hist_lighting_dist_1 = camera.get_history_texture("reblur_lighting_dist_1");
    auto hist_accumulation = camera.get_history_texture("reblur_accumulation");
    has_history &= hist_depth != gfx::TextureHandle::invalid
        && hist_normal_roughness != gfx::TextureHandle::invalid
        && hist_lighting_dist_0 != gfx::TextureHandle::invalid
        && hist_lighting_dist_1 != gfx::TextureHandle::invalid
        && hist_accumulation != gfx::TextureHandle::invalid;

    {
        auto [builder, pass_data] = rg.add_compute_pass<TemporalAccumulatePassData>("ReBLUR Temporal Accumulate");
        pass_data->velocity_tex = builder.read(input.velocity);
        pass_data->history_validation_tex = builder.read(input.history_validation);
        pass_data->depth_tex = builder.read(input.depth);
        pass_data->normal_roughness_tex = builder.read(input.gbuffer.normal_roughness);
        pass_data->in_lighting_dist_tex = builder.read(lighting_dist_1);
        pass_data->out_lighting_dist_tex = builder.write(lighting_dist_0);
        pass_data->accumulation_tex = builder.write(accumulation);
        if (has_history) {
            pass_data->hist_depth_tex = builder.read(hist_depth);
            pass_data->hist_normal_roughness_tex = builder.read(hist_normal_roughness);
            pass_data->hist_lighting_dist_tex = builder.read(hist_lighting_dist_0);
            pass_data->hist_accumulation_tex = builder.read(hist_accumulation);
        }
        builder.set_execution_function<TemporalAccumulatePassData>(
            [this, &camera, width, height, has_history](CRef<TemporalAccumulatePassData> pass_data, gfx::ComputePassContext const& ctx) {
                auto params = temporal_accumulate_shader_params_.mutable_typed_data<TemporalAccumulatePassParams>();
                params->tex_size = {width, height};
                params->gbuffer_tex_size = {width, height};
                params->virtual_history = 1;
                params->velocity_tex = {ctx.rg->texture(pass_data->velocity_tex)};
                params->history_validation_tex = {ctx.rg->texture(pass_data->history_validation_tex)};
                params->depth_tex = {ctx.rg->texture(pass_data->depth_tex)};
                params->normal_roughness_tex = {ctx.rg->texture(pass_data->normal_roughness_tex)};
                params->in_lighting_dist_tex = {ctx.rg->texture(pass_data->in_lighting_dist_tex)};
                params->out_lighting_dist_tex = {ctx.rg->texture(pass_data->out_lighting_dist_tex)};
                params->accumulation_tex = {ctx.rg->texture(pass_data->accumulation_tex)};
                if (has_history) {
                    params->hist_depth_tex = {ctx.rg->texture(pass_data->hist_depth_tex)};
                    params->hist_normal_roughness_tex = {ctx.rg->texture(pass_data->hist_normal_roughness_tex)};
                    params->hist_lighting_dist_tex = {ctx.rg->texture(pass_data->hist_lighting_dist_tex)};
                    params->hist_accumulation_tex = {ctx.rg->texture(pass_data->hist_accumulation_tex)};
                } else {
                    params->hist_depth_tex = {nullptr};
                    params->hist_normal_roughness_tex = {nullptr};
                    params->hist_lighting_dist_tex = {nullptr};
                    params->hist_accumulation_tex = {nullptr};
                }
                params->input_sampler = {sampler_.get()};
                temporal_accumulate_shader_params_.update_uniform_buffer();
                ctx.dispatch(
                    camera, temporal_accumulate_shader_, temporal_accumulate_shader_params_,
                    ceil_div(width, reblur_tile_size), ceil_div(height, reblur_tile_size)
                );
            }
        );
    }

    {
        auto [builder, pass_data] = rg.add_compute_pass<FetchLinearDepthPassData>("ReBLUR Fetch Linear Depth");
        pass_data->in_tex = builder.read(input.depth);
        pass_data->out_tex = builder.write(linear_depth);
        builder.set_execution_function<FetchLinearDepthPassData>(
            [this, &camera, width, height](CRef<FetchLinearDepthPassData> pass_data, gfx::ComputePassContext const& ctx) {
                auto params = fetch_linear_depth_shader_params_.mutable_typed_data<FetchLinearDepthPassParams>();
                params->tex_size = {width, height};
                params->in_tex = {ctx.rg->texture(pass_data->in_tex)};
                params->out_tex = {ctx.rg->texture(pass_data->out_tex)};
                fetch_linear_depth_shader_params_.update_uniform_buffer();
                ctx.dispatch(
                    camera, fetch_linear_depth_shader_, fetch_linear_depth_shader_params_,
                    ceil_div(width, reblur_tile_size), ceil_div(height, reblur_tile_size)
                );
            }
        );
    }

    {
        auto [builder, pass_data] = rg.add_compute_pass<GenDepthMipPassData>("ReBLUR Gen Depth Mip");
        pass_data->color_tex = builder.write(lighting_dist_0);
        lighting_dist_0 = pass_data->color_tex;
        pass_data->depth_tex = builder.write(linear_depth);
        linear_depth = pass_data->depth_tex;
        builder.set_execution_function<GenDepthMipPassData>(
            [this, &camera, width, height](CRef<GenDepthMipPassData> pass_data, gfx::ComputePassContext const& ctx) {
                auto params = gen_depth_mip_shader_params_.mutable_typed_data<GenDepthMipPassParams>();
                auto max_level = ctx.rg->texture(pass_data->depth_tex)->desc().levels - 1;
                params->tex_size = {width, height};
                params->in_depth_tex = {ctx.rg->texture(pass_data->depth_tex), 0};
                params->out_depth_tex_0 = {ctx.rg->texture(pass_data->depth_tex), std::min(1u, max_level)};
                params->out_depth_tex_1 = {ctx.rg->texture(pass_data->depth_tex), std::min(2u, max_level)};
                params->out_depth_tex_2 = {ctx.rg->texture(pass_data->depth_tex), std::min(3u, max_level)};
                params->in_tex = {ctx.rg->texture(pass_data->color_tex), 0};
                params->out_tex_0 = {ctx.rg->texture(pass_data->color_tex), std::min(1u, max_level)};
                params->out_tex_1 = {ctx.rg->texture(pass_data->color_tex), std::min(2u, max_level)};
                params->out_tex_2 = {ctx.rg->texture(pass_data->color_tex), std::min(3u, max_level)};
                gen_depth_mip_shader_params_.update_uniform_buffer();
                ctx.dispatch(
                    camera, gen_depth_mip_shader_, gen_depth_mip_shader_params_,
                    ceil_div(width, 16u), ceil_div(height, 16u)
                );
            }
        );
    }

    {
        auto [builder, pass_data] = rg.add_compute_pass<FixHistoryPassData>("ReBLUR Fix History");
        pass_data->depth_tex = builder.read(linear_depth);
        pass_data->in_lighting_dist_tex = builder.read(lighting_dist_0);
        pass_data->accumulation_tex = builder.read(accumulation);
        pass_data->normal_roughness_tex = builder.read(input.gbuffer.normal_roughness);
        pass_data->out_lighting_dist_tex = builder.write(lighting_dist_1);
        lighting_dist_1 = pass_data->out_lighting_dist_tex;
        builder.set_execution_function<FixHistoryPassData>(
            [this, &camera, width, height](CRef<FixHistoryPassData> pass_data, gfx::ComputePassContext const& ctx) {
                auto params = fix_history_shader_params_.mutable_typed_data<FixHistoryPassParams>();
                params->tex_size = {width, height};
                params->gbuffer_tex_size = {width, height};
                params->depth_tex = {ctx.rg->texture(pass_data->depth_tex)};
                params->in_lighting_dist_tex = {ctx.rg->texture(pass_data->in_lighting_dist_tex)};
                params->accumulation_tex = {ctx.rg->texture(pass_data->accumulation_tex)};
                params->normal_roughness_tex = {ctx.rg->texture(pass_data->normal_roughness_tex)};
                params->out_lighting_dist_tex = {ctx.rg->texture(pass_data->out_lighting_dist_tex)};
                fix_history_shader_params_.update_uniform_buffer();
                ctx.dispatch(
                    camera, fix_history_shader_, fix_history_shader_params_,
                    ceil_div(width, reblur_tile_size), ceil_div(height, reblur_tile_size)
                );
            }
        );
    }

    {
        auto [builder, pass_data] = rg.add_compute_pass<BlurPassData>("ReBLUR Blur");
        pass_data->depth_tex = builder.read(input.depth);
        pass_data->normal_roughness_tex = builder.read(input.gbuffer.normal_roughness);
        pass_data->accumulation_tex = builder.read(accumulation);
        pass_data->in_lighting_dist_tex = builder.read(lighting_dist_1);
        pass_data->out_lighting_dist_tex = builder.write(lighting_dist_0);
        lighting_dist_0 = pass_data->out_lighting_dist_tex;
        builder.set_execution_function<BlurPassData>(
            [this, &camera, width, height, rotator_index](CRef<BlurPassData> pass_data, gfx::ComputePassContext const& ctx) {
                auto params = blur_shader_params_.mutable_typed_data<BlurPassParams>();
                params->tex_size = {width, height};
                params->gbuffer_tex_size = {width, height};
                params->in_blur_radius = blur_radius;
                params->poisson_rotator = get_rotator(blur_rotator_rands[rotator_index]);
                params->depth_tex = {ctx.rg->texture(pass_data->depth_tex)};
                params->normal_roughness_tex = {ctx.rg->texture(pass_data->normal_roughness_tex)};
                params->accumulation_tex = {ctx.rg->texture(pass_data->accumulation_tex)};
                params->in_lighting_dist_tex = {ctx.rg->texture(pass_data->in_lighting_dist_tex)};
                params->out_lighting_dist_tex = {ctx.rg->texture(pass_data->out_lighting_dist_tex)};
                blur_shader_params_.update_uniform_buffer();
                ctx.dispatch(
                    camera, blur_shader_, blur_shader_params_,
                    ceil_div(width, reblur_tile_size), ceil_div(height, reblur_tile_size)
                );

                camera.add_history_texture("reblur_lighting_dist_0", pass_data->out_lighting_dist_tex);
                camera.add_history_texture("reblur_accumulation", pass_data->accumulation_tex);
            }
        );
    }

    {
        auto [builder, pass_data] = rg.add_compute_pass<TemporalStabilizePassData>("ReBLUR Temporal Stabilize");
        pass_data->depth_tex = builder.read(input.depth);
        pass_data->normal_roughness_tex = builder.read(input.gbuffer.normal_roughness);
        pass_data->velocity_tex = builder.read(input.velocity);
        pass_data->history_validation_tex = builder.read(input.history_validation);
        pass_data->in_lighting_dist_tex = builder.read(lighting_dist_0);
        pass_data->out_lighting_dist_tex = builder.write(lighting_dist_1);
        lighting_dist_1 = pass_data->out_lighting_dist_tex;
        if (has_history) {
            pass_data->hist_lighting_dist_tex = builder.read(hist_lighting_dist_1);
        }
        builder.set_execution_function<TemporalStabilizePassData>(
            [this, &camera, width, height, has_history](CRef<TemporalStabilizePassData> pass_data, gfx::ComputePassContext const &ctx) {
                auto params = temporal_stabilize_shader_params_.mutable_typed_data<TemporalStabilizePassParams>();
                params->tex_size = {width, height};
                params->gbuffer_tex_size = {width, height};
                params->anti_flickering_strength = anti_flickering_strength;
                params->depth_tex = {ctx.rg->texture(pass_data->depth_tex)};
                params->normal_roughness_tex = {ctx.rg->texture(pass_data->normal_roughness_tex)};
                params->velocity_tex = {ctx.rg->texture(pass_data->velocity_tex)};
                params->history_validation_tex = {ctx.rg->texture(pass_data->history_validation_tex)};
                params->in_lighting_dist_tex = {ctx.rg->texture(pass_data->in_lighting_dist_tex)};
                params->out_lighting_dist_tex = {ctx.rg->texture(pass_data->out_lighting_dist_tex)};
                if (has_history) {
                    params->hist_lighting_dist_tex = {ctx.rg->texture(pass_data->hist_lighting_dist_tex)};
                } else {
                    params->hist_lighting_dist_tex = {nullptr};
                }
                params->input_sampler = {sampler_.get()};
                temporal_stabilize_shader_params_.update_uniform_buffer();
                ctx.dispatch(
                    camera, temporal_stabilize_shader_, temporal_stabilize_shader_params_,
                    ceil_div(width, reblur_tile_size), ceil_div(height, reblur_tile_size)
                );

                camera.add_history_texture("reblur_lighting_dist_1", pass_data->out_lighting_dist_tex);
            }
        );
    }

    {
        auto [builder, pass_data] = rg.add_compute_pass<PostBlurPassData>("ReBLUR Post Blur");
        pass_data->depth_tex = builder.read(input.depth);
        pass_data->normal_roughness_tex = builder.read(input.gbuffer.normal_roughness);
        pass_data->accumulation_tex = builder.read(accumulation);
        pass_data->in_lighting_dist_tex = builder.read(lighting_dist_1);
        pass_data->out_lighting_dist_tex = builder.write(denoised);
        builder.set_execution_function<PostBlurPassData>(
            [this, &camera, width, height, rotator_index](CRef<PostBlurPassData> pass_data, gfx::ComputePassContext const& ctx) {
                auto params = post_blur_shader_params_.mutable_typed_data<PostBlurPassParams>();
                params->tex_size = {width, height};
                params->gbuffer_tex_size = {width, height};
                params->in_blur_radius = blur_radius;
                params->poisson_rotator = get_rotator(post_blur_rotator_rands[rotator_index]);
                params->depth_tex = {ctx.rg->texture(pass_data->depth_tex)};
                params->normal_roughness_tex = {ctx.rg->texture(pass_data->normal_roughness_tex)};
                params->accumulation_tex = {ctx.rg->texture(pass_data->accumulation_tex)};
                params->in_lighting_dist_tex = {ctx.rg->texture(pass_data->in_lighting_dist_tex)};
                params->out_lighting_dist_tex = {ctx.rg->texture(pass_data->out_lighting_dist_tex)};
                post_blur_shader_params_.update_uniform_buffer();
                ctx.dispatch(
                    camera, post_blur_shader_, post_blur_shader_params_,
                    ceil_div(width, reblur_tile_size), ceil_div(height, reblur_tile_size)
                );
            }
        );
    }

    return denoised;
}

}
