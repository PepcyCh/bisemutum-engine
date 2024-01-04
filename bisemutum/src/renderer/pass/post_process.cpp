#include "post_process.hpp"

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/component_utils.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>

namespace bi {

namespace {

BI_SHADER_PARAMETERS_BEGIN(PostProcessPassParams)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, input_color)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, input_depth)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, sampler_input)
BI_SHADER_PARAMETERS_END(PostProcessPassParams)

struct PostProcessPassData final {
    gfx::TextureHandle input_color;
    gfx::TextureHandle input_depth;

    gfx::TextureHandle output;
};

BI_SHADER_PARAMETERS_BEGIN(BloomPrePassParams)
    BI_SHADER_PARAMETER(float4, bloom_weight)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, input_color)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, sampler_input)
BI_SHADER_PARAMETERS_END(BloomPrePassParams)

struct BloomPrePassData final {
    gfx::TextureHandle input_color;
    gfx::TextureHandle output_color;
};

BI_SHADER_PARAMETERS_BEGIN(BloomFilterPassParams)
    BI_SHADER_PARAMETER(float2, texel_size)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, input_color)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, sampler_input)
BI_SHADER_PARAMETERS_END(BloomFilterPassParams)

struct BloomFilterPassData final {
    gfx::TextureHandle input_color;
    gfx::TextureHandle output_color;
};

BI_SHADER_PARAMETERS_BEGIN(BloomCombinePassParams)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, input_color1)
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2D, input_color2)
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, sampler_input)
BI_SHADER_PARAMETERS_END(BloomCombinePassParams)

struct BloomCombinePassData final {
    gfx::TextureHandle input_color1;
    gfx::TextureHandle input_color2;
    gfx::TextureHandle output_color;
};

}

PostProcessPass::PostProcessPass() {
    bloom_pre_shader_params.initialize<BloomPrePassParams>();
    bloom_pre_shader.source_path = "/bisemutum/shaders/renderer/bloom_pre.hlsl";
    bloom_pre_shader.source_entry = "bloom_pre_fs";
    bloom_pre_shader.set_shader_params_struct<BloomPrePassParams>();
    bloom_pre_shader.needed_vertex_attributes = gfx::VertexAttributesType::position_texcoord;
    bloom_pre_shader.depth_test = false;

    bloom_filter_shader[0].source_path = "/bisemutum/shaders/renderer/bloom_filter.hlsl";
    bloom_filter_shader[0].source_entry = "bloom_horizontal_fs";
    bloom_filter_shader[0].set_shader_params_struct<BloomFilterPassParams>();
    bloom_filter_shader[0].needed_vertex_attributes = gfx::VertexAttributesType::position_texcoord;
    bloom_filter_shader[0].depth_test = false;

    bloom_filter_shader[1].source_path = "/bisemutum/shaders/renderer/bloom_filter.hlsl";
    bloom_filter_shader[1].source_entry = "bloom_vertical_fs";
    bloom_filter_shader[1].set_shader_params_struct<BloomFilterPassParams>();
    bloom_filter_shader[1].needed_vertex_attributes = gfx::VertexAttributesType::position_texcoord;
    bloom_filter_shader[1].depth_test = false;

    bloom_combine_shader.source_path = "/bisemutum/shaders/renderer/bloom_combine.hlsl";
    bloom_combine_shader.source_entry = "bloom_combine_fs";
    bloom_combine_shader.set_shader_params_struct<BloomCombinePassParams>();
    bloom_combine_shader.needed_vertex_attributes = gfx::VertexAttributesType::position_texcoord;
    bloom_combine_shader.depth_test = false;

    fragmeng_shader_params.initialize<PostProcessPassParams>();
    fragmeng_shader.source_path = "/bisemutum/shaders/renderer/post_process.hlsl";
    fragmeng_shader.source_entry = "post_process_pass_fs";
    fragmeng_shader.set_shader_params_struct<PostProcessPassParams>();
    fragmeng_shader.needed_vertex_attributes = gfx::VertexAttributesType::position_texcoord;
    fragmeng_shader.depth_test = false;

    sampler = g_engine->graphics_manager()->get_sampler(rhi::SamplerDesc{
        .mag_filter = rhi::SamplerFilterMode::linear,
        .min_filter = rhi::SamplerFilterMode::linear,
        .address_mode_u = rhi::SamplerAddressMode::clamp_to_edge,
        .address_mode_v = rhi::SamplerAddressMode::clamp_to_edge,
        .address_mode_w = rhi::SamplerAddressMode::clamp_to_edge,
    });
}

auto PostProcessPass::find_volume(gfx::Camera const& camera) -> PostProcessVolumeComponent const& {
    auto volume = rt::find_volume_component_for<PostProcessVolumeComponent>(camera.position);
    return volume ? *volume : default_volume;
}

auto PostProcessPass::render(gfx::Camera const& camera, gfx::RenderGraph& rg, InputData const& input) -> void {
    auto& volume = find_volume(camera);

    gfx::TextureHandle color_after_bloom;
    if (volume.bloom) {
        constexpr uint32_t bloom_num_iterations = 3;

        auto width = camera.target_texture().desc().extent.width;
        auto height = camera.target_texture().desc().extent.height;
        auto color_before_bloom = rg.add_texture([width, height](gfx::TextureBuilder& builder) {
            builder
                .dim_2d(rhi::ResourceFormat::rgba16_sfloat, width, height)
                .usage({rhi::TextureUsage::color_attachment, rhi::TextureUsage::sampled});
        });
        {
            auto [builder, pd] = rg.add_graphics_pass<BloomPrePassData>(fmt::format("Bloom Pre Pass"));
            pd->input_color = builder.read(input.color);
            pd->output_color = builder.use_color(0, color_before_bloom);
            builder.set_execution_function<BloomPrePassData>(
                [&camera, &volume, this, &shader_params = bloom_pre_shader_params]
                (CRef<BloomPrePassData> pass_data, gfx::GraphicsPassContext const& ctx) {
                    auto soft_threshold = volume.bloom_threshold_softness * (volume.bloom_threshold * 0.9f + 0.1f);
                    auto params = shader_params.mutable_typed_data<BloomPrePassParams>();
                    params->bloom_weight.x = volume.bloom_threshold;
                    params->bloom_weight.y = volume.bloom_threshold * soft_threshold;
                    params->bloom_weight.z = 2.0f * params->bloom_weight.y;
                    params->bloom_weight.w = 0.25f / (params->bloom_weight.y + 0.00001f);
                    params->bloom_weight.y -= volume.bloom_threshold;
                    params->input_color = {ctx.rg->texture(pass_data->input_color)};
                    params->sampler_input = {sampler.value()};
                    shader_params.update_uniform_buffer();
                    ctx.render_full_screen(camera, bloom_pre_shader, shader_params);
                }
            );
        }

        color_after_bloom = rg.add_texture([width, height](gfx::TextureBuilder& builder) {
            builder
                .dim_2d(rhi::ResourceFormat::rgba16_sfloat, width, height)
                .usage({rhi::TextureUsage::color_attachment, rhi::TextureUsage::sampled});
        });

        std::vector<gfx::TextureHandle> temp_textures;
        if (bloom_filter_shader_params.empty()) {
            bloom_filter_shader_params.resize(bloom_num_iterations * 2);
            for (auto& params : bloom_filter_shader_params) {
                params.initialize<BloomFilterPassParams>();
            }
        }
        for (uint32_t i = 0; i < bloom_num_iterations; i++) {
            auto dst_width = std::max(width >> (i + 1), 1u);
            auto dst_height = std::max(height >> (i + 1), 1u);

            auto [builder_h, pd_h] =
                rg.add_graphics_pass<BloomFilterPassData>(fmt::format("Bloom Horizontal Pass #{}", i + 1));
            temp_textures.push_back(rg.add_texture([dst_width, dst_height](gfx::TextureBuilder& builder) {
                builder
                    .dim_2d(rhi::ResourceFormat::rgba16_sfloat, dst_width, dst_height)
                    .usage({rhi::TextureUsage::color_attachment, rhi::TextureUsage::sampled});
            }));
            pd_h->input_color = builder_h.read(i == 0 ? color_before_bloom : temp_textures[2 * i - 1]);
            pd_h->output_color = builder_h.use_color(0, temp_textures[2 * i]);
            builder_h.set_execution_function<BloomFilterPassData>(
                [&camera, i, this, dst_width, dst_height, &shader_params = bloom_filter_shader_params[2 * i]]
                (CRef<BloomFilterPassData> pass_data, gfx::GraphicsPassContext const& ctx) {
                    auto params = shader_params.mutable_typed_data<BloomFilterPassParams>();
                    params->input_color = {ctx.rg->texture(pass_data->input_color)};
                    params->sampler_input = {sampler.value()};
                    params->texel_size = float2{1.0f / dst_width, 1.0f / dst_height};
                    shader_params.update_uniform_buffer();
                    ctx.render_full_screen(camera, bloom_filter_shader[0], shader_params);
                }
            );

            auto [builder_v, pd_v] =
                rg.add_graphics_pass<BloomFilterPassData>(fmt::format("Bloom Vertical Pass #{}", i + 1));
            temp_textures.push_back(rg.add_texture([dst_width, dst_height](gfx::TextureBuilder& builder) {
                builder
                    .dim_2d(rhi::ResourceFormat::rgba16_sfloat, dst_width, dst_height)
                    .usage({rhi::TextureUsage::color_attachment, rhi::TextureUsage::sampled});
            }));
            pd_v->input_color = builder_v.read(temp_textures[2 * i]);
            pd_v->output_color = builder_v.use_color(0, temp_textures[2 * i + 1]);
            builder_v.set_execution_function<BloomFilterPassData>(
                [&camera, this, dst_width, dst_height, &shader_params = bloom_filter_shader_params[2 * i + 1]]
                (CRef<BloomFilterPassData> pass_data, gfx::GraphicsPassContext const& ctx) {
                    auto params = shader_params.mutable_typed_data<BloomFilterPassParams>();
                    params->input_color = {ctx.rg->texture(pass_data->input_color)};
                    params->sampler_input = {sampler.value()};
                    params->texel_size = float2{1.0f / dst_width, 1.0f / dst_height};
                    shader_params.update_uniform_buffer();
                    ctx.render_full_screen(camera, bloom_filter_shader[1], shader_params);
                }
            );
        }
        if (bloom_combine_shader_params.empty()) {
            bloom_combine_shader_params.resize(bloom_num_iterations);
            for (auto& params : bloom_combine_shader_params) {
                params.initialize<BloomCombinePassParams>();
            }
        }
        for (uint32_t i = bloom_num_iterations - 1; i > 0; i--) {
            auto dst_width = std::max(width >> i, 1u);
            auto dst_height = std::max(height >> i, 1u);

            auto [builder, pd] =
                rg.add_graphics_pass<BloomCombinePassData>(fmt::format("Bloom Combine Pass #{}", i));
            temp_textures.push_back(rg.add_texture([dst_width, dst_height](gfx::TextureBuilder& builder) {
                builder
                    .dim_2d(rhi::ResourceFormat::rgba16_sfloat, dst_width, dst_height)
                    .usage({rhi::TextureUsage::color_attachment, rhi::TextureUsage::sampled});
            }));
            pd->input_color1 = builder.read(temp_textures[2 * i - 1]);
            pd->input_color2 = builder.read(
                temp_textures[i == bloom_num_iterations - 1 ? 2 * i + 1 : 3 * bloom_num_iterations - i - 2]
            );
            pd->output_color = builder.use_color(0, temp_textures[3 * bloom_num_iterations - i - 1]);
            builder.set_execution_function<BloomCombinePassData>(
                [&camera, this, &shader_params = bloom_combine_shader_params[i]]
                (CRef<BloomCombinePassData> pass_data, gfx::GraphicsPassContext const& ctx) {
                    auto params = shader_params.mutable_typed_data<BloomCombinePassParams>();
                    params->input_color1 = {ctx.rg->texture(pass_data->input_color1)};
                    params->input_color2 = {ctx.rg->texture(pass_data->input_color2)};
                    params->sampler_input = {sampler.value()};
                    shader_params.update_uniform_buffer();
                    ctx.render_full_screen(camera, bloom_combine_shader, shader_params);
                }
            );
        }
        auto [builder, pd] = rg.add_graphics_pass<BloomCombinePassData>(fmt::format("Bloom Final Combine Pass"));
        pd->input_color1 = builder.read(input.color);
        pd->input_color2 = builder.read(temp_textures.back());
        pd->output_color = builder.use_color(0, color_after_bloom);
        builder.set_execution_function<BloomCombinePassData>(
            [&camera, this, &shader_params = bloom_combine_shader_params[0]]
            (CRef<BloomCombinePassData> pass_data, gfx::GraphicsPassContext const& ctx) {
                auto params = shader_params.mutable_typed_data<BloomCombinePassParams>();
                params->input_color1 = {ctx.rg->texture(pass_data->input_color1)};
                params->input_color2 = {ctx.rg->texture(pass_data->input_color2)};
                params->sampler_input = {sampler.value()};
                shader_params.update_uniform_buffer();
                ctx.render_full_screen(camera, bloom_combine_shader, shader_params);
            }
        );
    } else {
        color_after_bloom = input.color;
    }

    {
        auto [builder, pass_data] = rg.add_graphics_pass<PostProcessPassData>("Post Process Pass");

        pass_data->output = rg.import_back_buffer();
        builder.use_color(0, pass_data->output);

        pass_data->input_color = builder.read(color_after_bloom);
        pass_data->input_depth = builder.read(input.depth);

        builder.set_execution_function<PostProcessPassData>(
            [this, &camera](CRef<PostProcessPassData> pass_data, gfx::GraphicsPassContext const& ctx) {
                auto params = fragmeng_shader_params.mutable_typed_data<PostProcessPassParams>();
                params->input_color = {ctx.rg->texture(pass_data->input_color)};
                params->input_depth = {ctx.rg->texture(pass_data->input_depth)};
                params->sampler_input = {sampler.value()};
                fragmeng_shader_params.update_uniform_buffer();

                ctx.render_full_screen(camera, fragmeng_shader, fragmeng_shader_params);
            }
        );
    }
}

}
