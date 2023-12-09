#include "command_helpers.hpp"

#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/graphics/shader_param.hpp>
#include <bisemutum/runtime/logger.hpp>

namespace bi::gfx {

auto CommandHelpers::initialize(Ref<rhi::Device> device, ShaderCompiler& shader_compiler) -> void {
    device_ = device;

    initialize_blit(device, shader_compiler);
    initialize_mipmap(device, shader_compiler);
}

auto CommandHelpers::blit_2d(
    Ref<rhi::CommandEncoder> cmd_encoder,
    Ref<Texture> src, uint32_t src_mip_level, uint32_t src_array_layer,
    Ref<Texture> dst, uint32_t dst_mip_level, uint32_t dst_array_layer
) -> void {
    auto target_format = dst->desc().format;
    auto extent = dst->desc().extent;

    rhi::CommandLabel label{
        .color = {0.0f, 0.0f, 1.0f}
    };
    rhi::RenderTargetDesc rt_desc{};
    if (rhi::is_depth_stencil_format(target_format)) {
        label.label = "blit (depth)";
        rt_desc.depth_stencil = rhi::DepthStencilAttachmentDesc{
            .texture = {dst->rhi_texture()},
            .store = true,
        };
    } else {
        label.label = "blit (color)";
        rt_desc.colors.push_back(rhi::ColorAttachmentDesc{
            .texture = {dst->rhi_texture()},
            .store = true,
        });
    }

    auto pipeline = get_blit_pipeline(dst);
    auto graphics_encoder = cmd_encoder->begin_render_pass(label, rt_desc);
    rhi::Viewport viewport{
        .width = static_cast<float>(extent.width >> dst_mip_level),
        .height = static_cast<float>(extent.height >> dst_mip_level),
    };
    graphics_encoder->set_viewports({viewport});
    rhi::Scissor scissor{
        .width = extent.width >> dst_mip_level,
        .height = extent.height >> dst_mip_level,
    };
    graphics_encoder->set_scissors({scissor});
    graphics_encoder->set_pipeline(pipeline);

    auto descriptor = g_engine->graphics_manager()->get_gpu_descriptor_for(
        {src->get_srv(src_mip_level, 1, src_array_layer, 1)},
        pipeline->desc().bind_groups_layout[0]
    );
    graphics_encoder->set_descriptors(0, {descriptor});

    graphics_encoder->draw(3);
}

auto CommandHelpers::generate_mipmaps_2d(
    Ref<rhi::CommandEncoder> cmd_encoder,
    Ref<Texture> texture,
    BitFlags<rhi::ResourceAccessType>& texture_access,
    MipmapMode mode
)  -> void {
    auto target_format = texture->desc().format;

    rhi::ResourceAccessType read_access;
    rhi::ResourceAccessType write_access;
    std::function<auto(uint32_t) -> void> execute_func;
    uint32_t width = texture->desc().extent.width;
    uint32_t height = texture->desc().extent.height;
    if (rhi::is_depth_stencil_format(target_format)) {
        auto pipeline = get_mipmap_pipeline(texture, mode);
        read_access = rhi::ResourceAccessType::sampled_texture_read;
        write_access = rhi::ResourceAccessType::depth_stencil_attachment_write;

        execute_func = [&](uint32_t mip_level) {
            auto graphics_encoder = cmd_encoder->begin_render_pass(
                rhi::CommandLabel{"mipmap (depth)", {0.0f, 0.0f, 1.0f}},
                rhi::RenderTargetDesc{
                    .depth_stencil = rhi::DepthStencilAttachmentDesc{
                        .texture = {texture->rhi_texture(), mip_level + 1},
                        .store = true,
                    }
                }
            );
            rhi::Viewport viewport{
                .width = static_cast<float>(width),
                .height = static_cast<float>(height),
            };
            graphics_encoder->set_viewports({viewport});
            rhi::Scissor scissor{
                .width = width,
                .height = height,
            };
            graphics_encoder->set_scissors({scissor});
            graphics_encoder->set_pipeline(pipeline);

            auto descriptor = g_engine->graphics_manager()->get_gpu_descriptor_for(
                {texture->get_srv(mip_level, 1)},
                pipeline->desc().bind_groups_layout[0]
            );
            graphics_encoder->set_descriptors(0, {descriptor});
            uint2 size{width, height};
            graphics_encoder->push_constants(&size, sizeof(size));

            graphics_encoder->draw(3);
        };
    } else if (rhi::is_compressed_format(target_format) || rhi::is_srgb_format(target_format)) {
        auto pipeline = get_mipmap_pipeline(texture, mode);
        read_access = rhi::ResourceAccessType::sampled_texture_read;
        write_access = rhi::ResourceAccessType::color_attachment_write;

        execute_func = [&](uint32_t mip_level) {
            auto graphics_encoder = cmd_encoder->begin_render_pass(
                rhi::CommandLabel{"mipmap (color graphics)", {0.0f, 0.0f, 1.0f}},
                rhi::RenderTargetDesc{
                    .colors = {
                        rhi::ColorAttachmentDesc{
                            .texture = {texture->rhi_texture(), mip_level + 1},
                            .store = true,
                        },
                    }
                }
            );
            rhi::Viewport viewport{
                .width = static_cast<float>(width),
                .height = static_cast<float>(height),
            };
            graphics_encoder->set_viewports({viewport});
            rhi::Scissor scissor{
                .width = width,
                .height = height,
            };
            graphics_encoder->set_scissors({scissor});
            graphics_encoder->set_pipeline(pipeline);

            auto descriptor = g_engine->graphics_manager()->get_gpu_descriptor_for(
                {texture->get_srv(mip_level, 1)},
                pipeline->desc().bind_groups_layout[0]
            );
            graphics_encoder->set_descriptors(0, {descriptor});
            uint2 size{width, height};
            graphics_encoder->push_constants(&size, sizeof(size));

            graphics_encoder->draw(3);
        };
    } else {
        auto pipeline = mipmap_pipelines_compute_[static_cast<size_t>(mode)].ref();
        read_access = rhi::ResourceAccessType::sampled_texture_read;
        write_access = rhi::ResourceAccessType::storage_resource_write;

        execute_func = [&](uint32_t mip_level) {
            auto compute_encoder = cmd_encoder->begin_compute_pass(
                rhi::CommandLabel{"mipmap (color compute)", {1.0f, 0.0f, 0.0f}}
            );
            compute_encoder->set_pipeline(pipeline);

            auto descriptor = g_engine->graphics_manager()->get_gpu_descriptor_for(
                {texture->get_srv(mip_level, 1), texture->get_uav(mip_level + 1)},
                pipeline->desc().bind_groups_layout[0]
            );
            compute_encoder->set_descriptors(0, {descriptor});
            uint2 size{width, height};
            compute_encoder->push_constants(&size, sizeof(size));

            compute_encoder->dispatch((width + 15) / 16, (height + 15) / 16, 1);
        };
    }

    auto num_levels = std::min<uint32_t>(std::log2(std::max(width, height)) + 1, texture->desc().levels);
    for (uint32_t level = 0; level + 1 < num_levels; level++) {
        cmd_encoder->resource_barriers({}, {
            rhi::TextureBarrier{
                .texture = texture->rhi_texture(),
                .base_level = level,
                .num_levels = 1,
                .src_access_type = level == 0 ? texture_access : write_access,
                .dst_access_type = read_access,
            },
            rhi::TextureBarrier{
                .texture = texture->rhi_texture(),
                .base_level = level + 1,
                .num_levels = 1,
                .src_access_type = texture_access,
                .dst_access_type = write_access,
            },
        });
        width /= 2;
        height /= 2;
        execute_func(level);
    }
    if (num_levels > 1) {
        cmd_encoder->resource_barriers({}, {
            rhi::TextureBarrier{
                .texture = texture->rhi_texture(),
                .base_level = num_levels - 1,
                .num_levels = 1,
                .src_access_type = write_access,
                .dst_access_type = read_access,
            },
        });
    } else if (texture_access != read_access) {
        cmd_encoder->resource_barriers({}, {
            rhi::TextureBarrier{
                .texture = texture->rhi_texture(),
                .src_access_type = texture_access,
                .dst_access_type = read_access,
            },
        });
    }
    texture_access = read_access;
}

auto CommandHelpers::initialize_blit(Ref<rhi::Device> device, ShaderCompiler& shader_compiler) -> void {
    blit_sampler_ = device->create_sampler(rhi::SamplerDesc{
        .mag_filter = rhi::SamplerFilterMode::linear,
        .min_filter = rhi::SamplerFilterMode::linear,
    });

    auto source_path = "/bisemutum/shaders/core/blit.hlsl";
    ShaderCompilationEnvironment shader_env;
    auto blit_vs = shader_compiler.compile_shader(source_path, "blit_vs", rhi::ShaderStage::vertex, shader_env);
    BI_ASSERT_MSG(blit_vs.has_value(), blit_vs.error());
    blit_vs_ = blit_vs.value();
    auto blit_fs = shader_compiler.compile_shader(source_path, "blit_fs", rhi::ShaderStage::fragment, shader_env);
    BI_ASSERT_MSG(blit_fs.has_value(), blit_fs.error());
    blit_fs_ = blit_fs.value();

    shader_env.set_define("BLIT_DEPTH");
    auto blit_fs_depth = shader_compiler.compile_shader(source_path, "blit_fs", rhi::ShaderStage::fragment, shader_env);
    BI_ASSERT_MSG(blit_fs_depth.has_value(), blit_fs_depth.error());
    blit_fs_depth_ = blit_fs_depth.value();
}
auto CommandHelpers::get_blit_pipeline(Ref<Texture> dst_texture) -> Ref<rhi::GraphicsPipeline> {
    auto target_format = dst_texture->desc().format;
    if (auto it = blit_pipelines_.find(target_format); it != blit_pipelines_.end()) {
        return it->second.ref();
    }

    rhi::BindGroupLayout blit_layout{
        rhi::BindGroupLayoutEntry{
            .count = 1,
            .type = rhi::DescriptorType::sampled_texture,
            .visibility = rhi::ShaderStage::fragment,
            .binding_or_register = 0,
            .space = 0,
        },
    };
    rhi::StaticSampler blit_static_sampler{
        .sampler = blit_sampler_.ref(),
        .binding_or_register = 0,
        .space = 1,
        .visibility = rhi::ShaderStage::fragment,
    };

    if (rhi::is_depth_stencil_format(target_format)) {
        rhi::GraphicsPipelineDesc pipeline_desc{
            .bind_groups_layout = {std::move(blit_layout)},
            .static_samplers = {std::move(blit_static_sampler)},
            .depth_stencil_state = {
                .format = target_format,
            },
            .shaders = {
                .vertex = {blit_vs_.value(), "blit_vs"},
                .fragment = {blit_fs_depth_.value(), "blit_fs"},
            },
        };
        auto it = blit_pipelines_.insert({target_format, device_->create_graphics_pipeline(pipeline_desc)}).first;
        return it->second.ref();
    } else {
        rhi::GraphicsPipelineDesc pipeline_desc{
            .bind_groups_layout = {std::move(blit_layout)},
            .static_samplers = {std::move(blit_static_sampler)},
            .color_target_state = {
                .attachments = {
                    rhi::ColorTargetAttachmentState{
                        .format = target_format,
                    },
                },
            },
            .shaders = {
                .vertex = {blit_vs_.value(), "blit_vs"},
                .fragment = {blit_fs_.value(), "blit_fs"},
            },
        };
        auto it = blit_pipelines_.insert({target_format, device_->create_graphics_pipeline(pipeline_desc)}).first;
        return it->second.ref();
    }
}

auto CommandHelpers::initialize_mipmap(Ref<rhi::Device> device, ShaderCompiler& shader_compiler) -> void {
    for (size_t i = 0; i < 3; i++) {
        auto source_path = "/bisemutum/shaders/core/mipmap.hlsl";
        ShaderCompilationEnvironment shader_env;
        shader_env.set_define("MIPMAP_MODE", std::to_string(i));
        if (i == 0) {
            auto mipmap_vs = shader_compiler.compile_shader(
                source_path, "mipmap_vs", rhi::ShaderStage::vertex, shader_env
            );
            BI_ASSERT_MSG(mipmap_vs.has_value(), mipmap_vs.error());
            mipmap_vs_ = mipmap_vs.value();
        }
        auto mipmap_fs = shader_compiler.compile_shader(
            source_path, "mipmap_fs", rhi::ShaderStage::fragment, shader_env
        );
        BI_ASSERT_MSG(mipmap_fs.has_value(), mipmap_fs.error());
        mipmap_fs_[i] = mipmap_fs.value();

        shader_env.set_define("MIPMAP_DEPTH");
        auto mipmap_fs_depth = shader_compiler.compile_shader(
            source_path, "mipmap_fs", rhi::ShaderStage::fragment, shader_env
        );
        BI_ASSERT_MSG(mipmap_fs_depth.has_value(), mipmap_fs_depth.error());
        mipmap_fs_depth_[i] = mipmap_fs_depth.value();

        shader_env.reset_define("MIPMAP_DEPTH");
        shader_env.set_define("USE_CS");
        auto mipmap_cs = shader_compiler.compile_shader(
            source_path, "mipmap_cs", rhi::ShaderStage::compute, shader_env
        );
        BI_ASSERT_MSG(mipmap_cs.has_value(), mipmap_cs.error());

        rhi::BindGroupLayout mipmap_layout{
            rhi::BindGroupLayoutEntry{
                .count = 1,
                .type = rhi::DescriptorType::sampled_texture,
                .visibility = rhi::ShaderStage::compute,
                .binding_or_register = 1,
                .space = 0,
            },
            rhi::BindGroupLayoutEntry{
                .count = 1,
                .type = rhi::DescriptorType::read_write_storage_texture,
                .visibility = rhi::ShaderStage::compute,
                .binding_or_register = 2,
                .space = 0,
            },
        };
        rhi::ComputePipelineDesc pipeline_desc{
            .bind_groups_layout = {std::move(mipmap_layout)},
            .push_constants = {
                .size = sizeof(uint2),
                .visibility = rhi::ShaderStage::compute,
                .register_ = 0,
                .space = 0,
            },
            .compute = {mipmap_cs.value(), "mipmap_cs"},
        };
        mipmap_pipelines_compute_[i] = device->create_compute_pipeline(pipeline_desc);
    }
}
auto CommandHelpers::get_mipmap_pipeline(Ref<Texture> dst_texture, MipmapMode mode) -> Ref<rhi::GraphicsPipeline> {
    auto key = std::make_pair(dst_texture->desc().format, mode);
    if (auto it = mipmap_pipelines_.find(key); it != mipmap_pipelines_.end()) {
        return it->second.ref();
    }

    rhi::BindGroupLayout mipmap_layout{
        rhi::BindGroupLayoutEntry{
            .count = 1,
            .type = rhi::DescriptorType::sampled_texture,
            .visibility = rhi::ShaderStage::fragment,
            .binding_or_register = 1,
            .space = 0,
        },
    };
    rhi::PushConstantsDesc push_constants{
        .size = sizeof(uint2),
        .visibility = rhi::ShaderStage::fragment,
        .register_ = 0,
        .space = 0,
    };
    if (rhi::is_depth_stencil_format(key.first)) {
        rhi::GraphicsPipelineDesc pipeline_desc{
            .bind_groups_layout = {std::move(mipmap_layout)},
            .push_constants = push_constants,
            .depth_stencil_state = {
                .format = key.first,
            },
            .shaders = {
                .vertex = {mipmap_vs_.value(), "mipmap_vs"},
                .fragment = {mipmap_fs_depth_[static_cast<size_t>(key.second)].value(), "mipmap_fs"},
            },
        };
        auto it = mipmap_pipelines_.insert({key, device_->create_graphics_pipeline(pipeline_desc)}).first;
        return it->second.ref();
    } else {
        rhi::GraphicsPipelineDesc pipeline_desc{
            .bind_groups_layout = {std::move(mipmap_layout)},
            .push_constants = push_constants,
            .color_target_state = {
                .attachments = {
                    rhi::ColorTargetAttachmentState{
                        .format = key.first,
                    },
                },
            },
            .shaders = {
                .vertex = {mipmap_vs_.value(), "mipmap_vs"},
                .fragment = {mipmap_fs_[static_cast<size_t>(key.second)].value(), "mipmap_fs"},
            },
        };
        auto it = mipmap_pipelines_.insert({key, device_->create_graphics_pipeline(pipeline_desc)}).first;
        return it->second.ref();
    }
}

}
