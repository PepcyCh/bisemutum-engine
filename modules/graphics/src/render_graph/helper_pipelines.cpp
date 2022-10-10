#include "render_graph/helper_pipelines.hpp"

#include <fstream>
#include <filesystem>

#include "core/utils.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

HelperPipelines::HelperPipelines(Ref<gfx::Device> device) : device_(device) {
    shader_manager_ = Ptr<ShaderManager>::Make(device, CurrentExecutablePath() / "shader_binary");
    
    InitBlitPipelines();
    InitMipmapPipelines();
}

void HelperPipelines::BlitTexture(Ref<gfx::CommandEncoder> cmd_encoder, const gfx::TextureView &src_view,
    const gfx::TextureView &dst_view) const {
    gfx::ResourceFormat dst_format = dst_view.texture->Desc().format;

    if (gfx::IsDepthStencilFormat(dst_format)) {
        gfx::RenderTargetDesc rt_desc {
            .depth_stencil = gfx::DepthStencilAttachmentDesc {
                .texture = dst_view,
                .store = true,
            }
        };
        auto render_encoder = cmd_encoder->BeginRenderPass({ "blit depth" }, rt_desc);
        render_encoder->SetPipeline(blit_pipeline_depth_);
        render_encoder->BindShaderParams(0, { { src_view } });
        render_encoder->BindShaderParams(1, { { std::monostate {} } });
        render_encoder->Draw(3);
    } else if (gfx::IsSrgbFormat(dst_format)) {
        gfx::RenderTargetDesc rt_desc {
            .colors = {
                gfx::ColorAttachmentDesc {
                    .texture = dst_view,
                    .store = true,
                }
            }
        };
        auto render_encoder = cmd_encoder->BeginRenderPass({ "blit" }, rt_desc);
        render_encoder->SetPipeline(blit_pipeline_);
        render_encoder->BindShaderParams(0, { { src_view } });
        render_encoder->BindShaderParams(1, { { std::monostate {} } });
        render_encoder->Draw(3);
    }
}

void HelperPipelines::GenerateMipmaps2D(Ref<gfx::CommandEncoder> cmd_encoder, Ref<gfx::Texture> texture,
    gfx::ResourceAccessType &tex_access_type, MipmapMode mode) const {
    const auto &texture_desc = texture->Desc();
    gfx::ResourceFormat format = texture_desc.format;
    uint32_t width = texture_desc.extent.width;
    uint32_t height = texture_desc.extent.height;

    gfx::ResourceAccessType read_access_type;
    gfx::ResourceAccessType write_access_type;
    std::function<void(const gfx::TextureView &, const gfx::TextureView &)> execute_func;
    if (gfx::IsDepthStencilFormat(format)) {
        auto pipeline = mode == MipmapMode::eAverage ? mipmap_pipeline_avg_depth_.AsRef()
            : mode == MipmapMode::eMin ? mipmap_pipeline_min_depth_.AsRef() : mipmap_pipeline_max_depth_.AsRef();

        read_access_type = gfx::ResourceAccessType::eRenderShaderSampledTextureRead;
        write_access_type = gfx::ResourceAccessType::eDepthStencilAttachmentWrite;

        execute_func = [&](const gfx::TextureView &src_view, const gfx::TextureView &dst_view) {
            gfx::RenderTargetDesc rt_desc {
                .depth_stencil = gfx::DepthStencilAttachmentDesc {
                    .texture = dst_view,
                    .store = true,
                }
            };
            auto render_encoder = cmd_encoder->BeginRenderPass({ "mipmap depth "}, rt_desc);
            render_encoder->SetPipeline(pipeline);
            render_encoder->BindShaderParams(0, { {
                std::monostate {},
                src_view,
            } });
            uint32_t tex_size[2] = { width, height };
            render_encoder->PushConstants(tex_size, 2 * sizeof(uint32_t));
            render_encoder->Draw(3);
        };
    } else if (gfx::IsSrgbFormat(format)) {
        auto pipeline = mode == MipmapMode::eAverage ? mipmap_pipeline_avg_render_.AsRef()
            : mode == MipmapMode::eMin ? mipmap_pipeline_min_render_.AsRef() : mipmap_pipeline_max_render_.AsRef();

        read_access_type = gfx::ResourceAccessType::eRenderShaderSampledTextureRead;
        write_access_type = gfx::ResourceAccessType::eColorAttachmentWrite;

        execute_func = [&](const gfx::TextureView &src_view, const gfx::TextureView &dst_view) {
            gfx::RenderTargetDesc rt_desc {
                .colors = {
                    gfx::ColorAttachmentDesc {
                        .texture = dst_view,
                        .store = true,
                    }
                }
            };
            auto render_encoder = cmd_encoder->BeginRenderPass({ "mipmap render "}, rt_desc);
            render_encoder->SetPipeline(pipeline);
            render_encoder->BindShaderParams(0, { {
                std::monostate {},
                src_view,
            } });
            uint32_t tex_size[2] = { width, height };
            render_encoder->PushConstants(tex_size, 2 * sizeof(uint32_t));
            render_encoder->Draw(3);
        };
    } else {
        auto pipeline = mode == MipmapMode::eAverage ? mipmap_pipeline_avg_compute_.AsRef()
            : mode == MipmapMode::eMin ? mipmap_pipeline_min_compute_.AsRef() : mipmap_pipeline_max_compute_.AsRef();

        read_access_type = gfx::ResourceAccessType::eComputeShaderSampledTextureRead;
        write_access_type = gfx::ResourceAccessType::eComputeShaderStorageResourceWrite;

        execute_func = [&](const gfx::TextureView &src_view, const gfx::TextureView &dst_view) {
            auto compute_encoder = cmd_encoder->BeginComputePass({ "mipmap compute "});
            compute_encoder->SetPipeline(pipeline);
            compute_encoder->BindShaderParams(0, { {
                std::monostate {},
                src_view,
                dst_view,
            } });
            uint32_t tex_size[2] = { width, height };
            compute_encoder->PushConstants(tex_size, 2 * sizeof(uint32_t));
            compute_encoder->Dispatch(width, height, 1);
        };
    }

    uint32_t level = 0;
    while (width > 1 || height > 1) {
        gfx::TextureView src_view {
            .texture = texture,
            .base_level = level,
            .levels = 1,
        };
        gfx::TextureBarrier src_barrier {
            .texture = src_view,
            .src_access_type = level == 0 ? tex_access_type : write_access_type,
            .dst_access_type = read_access_type,
        };
        gfx::TextureView dst_view {
            .texture = texture,
            .base_level = level + 1,
            .levels = 1,
        };
        gfx::TextureBarrier dst_barrier {
            .texture = dst_view,
            .src_access_type = tex_access_type,
            .dst_access_type = write_access_type,
        };
        cmd_encoder->ResourceBarrier({}, { src_barrier, dst_barrier });

        execute_func(src_view, dst_view);

        width /= 2;
        height /= 2;
        ++level;
    }

    gfx::TextureView final_view {
        .texture = texture,
        .base_level = level,
        .levels = 1,
    };
    gfx::TextureBarrier final_barrier {
        .texture = final_view,
        .src_access_type = write_access_type,
        .dst_access_type = read_access_type,
    };
    cmd_encoder->ResourceBarrier({}, { final_barrier });
    tex_access_type = read_access_type;
}

void HelperPipelines::InitBlitPipelines() {
    SamplerDesc sampler_desc {
        .mag_filter = SamplerFilterMode::eLinear,
        .min_filter = SamplerFilterMode::eLinear,
    };
    blit_sampler_ = device_->CreateSampler(sampler_desc);

    auto blit_vs = shader_manager_->GetShaderModule("gfx-helper_piplines-blit-vs",
        fs::path(kModuleDirectory) / "shaders/blit.hlsl", "VS", ShaderStage::eVertex);
    auto blit_fs = shader_manager_->GetShaderModule("gfx-helper_piplines-blit-fs",
        fs::path(kModuleDirectory) / "shaders/blit.hlsl", "FS", ShaderStage::eFragment);
    HashMap<std::string, std::string> defines;
    defines["BLIT_DEPTH"] = "";
    auto blit_fs_depth = shader_manager_->GetShaderModule("gfx-helper_piplines-blit-fs",
        fs::path(kModuleDirectory) / "shaders/blit.hlsl", "FS", ShaderStage::eFragment, defines);

    PipelineLayout blit_layout {
        .sets_layout = {
            DescriptorSetLayout {
                .bindings = {
                    DescriptorSetLayoutBinding {
                        .type = DescriptorType::eSampledTexture,
                        .tex_dim = gfx::TextureViewDimension::e2D,
                        .count = 1,
                    },
                }
            },
            DescriptorSetLayout {
                .bindings = {
                    DescriptorSetLayoutBinding {
                        .type = DescriptorType::eSampler,
                        .count = 1,
                        .immutable_samplers = { blit_sampler_.AsRef() },
                    },
                }
            },
        },
        .push_constants_size = 0,
    };
    RenderPipelineDesc blit_desc {
        .name = "blit",
        .layout = blit_layout,
        .vertex_input_buffers = {},
        .primitive_state = {},
        .depth_stencil_state = {},
        .color_target_state = ColorTargetState {
            .attachments = { {} }
        },
        .shaders = {
            .vertex = blit_vs.AsRef(),
            .fragment = blit_fs.AsRef(),
        }
    };
    blit_pipeline_ = device_->CreateRenderPipeline(blit_desc);
    RenderPipelineDesc blit_depth_desc {
        .name = "blit depth",
        .layout = blit_layout,
        .vertex_input_buffers = {},
        .primitive_state = {},
        .depth_stencil_state = {},
        .color_target_state = {},
        .shaders = {
            .vertex = blit_vs.AsRef(),
            .fragment = blit_fs_depth.AsRef(),
        }
    };
    blit_pipeline_depth_ = device_->CreateRenderPipeline(blit_depth_desc);
}

void HelperPipelines::InitMipmapPipelines() {
    for (int i = 0; i < 3; i++) {
        HashMap<std::string, std::string> defines;
        defines["MIPMAP_MODE"] = std::to_string(i);

        auto mipmap_vs = shader_manager_->GetShaderModule("gfx-helper_piplines-mipmap-vs",
            fs::path(kModuleDirectory) / "shaders/mipmap.hlsl", "VS", ShaderStage::eVertex, defines);
        auto mipmap_fs = shader_manager_->GetShaderModule("gfx-helper_piplines-mipmap-fs",
            fs::path(kModuleDirectory) / "shaders/mipmap.hlsl", "FS", ShaderStage::eFragment, defines);
        defines["MIPMAP_DEPTH"] = "";
        auto mipmap_fs_depth = shader_manager_->GetShaderModule("gfx-helper_piplines-mipmap-fs",
            fs::path(kModuleDirectory) / "shaders/mipmap.hlsl", "FS", ShaderStage::eFragment, defines);

        defines.erase("MIPMAP_DEPTH");
        defines["USE_CS"] = "";
        auto mipmap_cs = shader_manager_->GetShaderModule("gfx-helper_piplines-mipmap-cs",
            fs::path(kModuleDirectory) / "shaders/mipmap.hlsl", "CS", ShaderStage::eCompute, defines);

        PipelineLayout mipmap_layout {
            .sets_layout = {
                DescriptorSetLayout {
                    .bindings = {
                        DescriptorSetLayoutBinding { .type = gfx::DescriptorType::eNone },
                        DescriptorSetLayoutBinding {
                            .type = DescriptorType::eSampledTexture,
                            .tex_dim = gfx::TextureViewDimension::e2D,
                            .count = 1,
                        },
                    }
                },
            },
            .push_constants_size = sizeof(uint32_t) * 2,
        };
        RenderPipelineDesc mipmap_render_desc {
            .name = "mipmap render",
            .layout = mipmap_layout,
            .vertex_input_buffers = {},
            .primitive_state = {},
            .depth_stencil_state = {},
            .color_target_state = ColorTargetState {
                .attachments = { {} }
            },
            .shaders = {
                .vertex = mipmap_vs.AsRef(),
                .fragment = mipmap_fs.AsRef(),
            }
        };
        if (i == 0) {
            mipmap_pipeline_avg_render_ = device_->CreateRenderPipeline(mipmap_render_desc);
        } else if (i == 1) {
            mipmap_pipeline_min_render_ = device_->CreateRenderPipeline(mipmap_render_desc);
        } else {
            mipmap_pipeline_max_render_ = device_->CreateRenderPipeline(mipmap_render_desc);
        }
        RenderPipelineDesc mipmap_depth_desc {
            .name = "mipmap depth",
            .layout = mipmap_layout,
            .vertex_input_buffers = {},
            .primitive_state = {},
            .depth_stencil_state = {},
            .color_target_state = ColorTargetState {},
            .shaders = {
                .vertex = mipmap_vs.AsRef(),
                .fragment = mipmap_fs_depth.AsRef(),
            }
        };
        if (i == 0) {
            mipmap_pipeline_avg_depth_ = device_->CreateRenderPipeline(mipmap_depth_desc);
        } else if (i == 1) {
            mipmap_pipeline_min_depth_ = device_->CreateRenderPipeline(mipmap_depth_desc);
        } else {
            mipmap_pipeline_max_depth_ = device_->CreateRenderPipeline(mipmap_depth_desc);
        }

        mipmap_layout.sets_layout[0].bindings.push_back(DescriptorSetLayoutBinding {
            .type = DescriptorType::eRWStorageTexture,
            .tex_dim = gfx::TextureViewDimension::e2D,
            .count = 1,
        });
        ComputePipelineDesc mipmap_compute_desc {
            .name = "mipmap compute",
            .layout = mipmap_layout,
            .thread_group = { 16, 16, 1 },
            .compute = mipmap_cs.AsRef(),
        };
        if (i == 0) {
            mipmap_pipeline_avg_compute_ = device_->CreateComputePipeline(mipmap_compute_desc);
        } else if (i == 1) {
            mipmap_pipeline_min_compute_ = device_->CreateComputePipeline(mipmap_compute_desc);
        } else {
            mipmap_pipeline_max_compute_ = device_->CreateComputePipeline(mipmap_compute_desc);
        }
    }
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
