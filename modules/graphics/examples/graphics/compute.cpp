#include <iostream>

#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <core/module_manager.hpp>
#include <graphics/examples.hpp>
#include <graphics/device.hpp>
#include <graphics/pipeline.hpp>
#include <graphics/shader_compiler.hpp>

using namespace bismuth;

static constexpr uint32_t kNumFrames = 3;

int main(int argc, char **argv) {
    gfx::GraphicsBackend backend = gfx::GraphicsBackend::eVulkan;
#ifdef WIN32
    for (int i = 1; i < argc; i++) {
        if (i + 1 < argc && (strcmp("--backend", argv[i]) == 0 || strcmp("-b", argv[i]) == 0)) {
            ++i;
            if (strcmp("d3d12", argv[i]) == 0) {
                backend = gfx::GraphicsBackend::eD3D12;
            } else if (strcmp("vulkan", argv[i]) == 0) {
                backend = gfx::GraphicsBackend::eVulkan;
            } else {
                BI_WARN(gGeneralLogger, "unknown backend '{}'", argv[i]);
                return -1;
            }
        } else {
            BI_WARN(gGeneralLogger, "unknown argument '{}'", argv[i]);
            return -1;
        }
    }
#endif
    BI_INFO(gGeneralLogger, "Use backend: {}", backend == gfx::GraphicsBackend::eVulkan ? "Vulkan" : "D3D12");

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(1280, 720, "Bismuth", nullptr, nullptr);

    ModuleManager::Load<gfx::GraphicsModule>();

    gfx::DeviceDesc device_desc {
        .backend = backend,
        .enable_validation = true,
        .window = window,
    };
    auto device = gfx::Device::Create(device_desc);

    auto graphics_queue = device->GetQueue(gfx::QueueType::eGraphics);
    auto context = device->CreateFrameContext();

    int texture_width, texture_height, texture_channels;
    auto texture_path = std::filesystem::path(kExamplesDir) / "graphics/texture.png";
    uint8_t *texture_data = stbi_load(texture_path.string().c_str(), &texture_width, &texture_height,
        &texture_channels, 0);
    size_t texture_data_size = texture_width * texture_height * texture_channels * sizeof(uint8_t);
    gfx::ResourceFormat texture_format =
        texture_channels == 3 ? gfx::ResourceFormat::eRgb8UNorm : gfx::ResourceFormat::eRgba8UNorm;
    gfx::TextureDesc texture_desc {
        .name = "texture 1",
        .extent = { static_cast<uint32_t>(texture_width), static_cast<uint32_t>(texture_height) },
        .levels = 1,
        .format = texture_format,
        .dim = gfx::TextureDimension::e2D,
        .usages = { gfx::TextureUsage::eRWStorage, gfx::TextureUsage::eSampled },
    };
    auto texture1 = device->CreateTexture(texture_desc);
    texture_desc.name = "texture 2";
    auto texture2 = device->CreateTexture(texture_desc);
    {
        context->Reset();
        auto cmd_encoder = context->GetCommandEncoder();
        
        gfx::BufferDesc texture_upload_buffer_desc {
            .name = "texture upload buffer",
            .size = texture_data_size,
            .usages = {},
            .memory_property = gfx::BufferMemoryProperty::eCpuToGpu,
        };
        auto texture_upload_buffer = device->CreateBuffer(texture_upload_buffer_desc);

        gfx::BufferBarrier buffer_barrier {
            .buffer = texture_upload_buffer.AsRef(),
            .src_access_type = gfx::ResourceAccessType::eNone,
            .dst_access_type = gfx::ResourceAccessType::eTransferRead,
        };
        gfx::TextureBarrier texture1_barrier {
            .texture = gfx::TextureView { texture1.AsRef() },
            .src_access_type = gfx::ResourceAccessType::eNone,
            .dst_access_type = gfx::ResourceAccessType::eTransferWrite,
        };
        
        cmd_encoder->ResourceBarrier({ buffer_barrier }, { texture1_barrier });

        void *mapped_ptr = texture_upload_buffer->Map();
        memcpy(mapped_ptr, texture_data, texture_data_size);
        texture_upload_buffer->Unmap();
        gfx::BufferTextureCopyDesc copy_desc {
            .buffer_offset = 0,
            .buffer_bytes_per_row = static_cast<uint32_t>(texture_width * texture_channels * sizeof(uint8_t)),
            .buffer_rows_per_texture = static_cast<uint32_t>(texture_height),
            .texture_offset = {},
            .texture_extent = texture_desc.extent,
            .texture_level = 0,
        };
        cmd_encoder->CopyBufferToTexture(texture_upload_buffer.AsRef(), texture1.AsRef(), { copy_desc });

        texture1_barrier = gfx::TextureBarrier {
            .texture = gfx::TextureView { texture1.AsRef() },
            .src_access_type = gfx::ResourceAccessType::eTransferWrite,
            .dst_access_type = gfx::ResourceAccessType::eComputeShaderSampledTextureRead,
        };
        gfx::TextureBarrier texture2_barrier {
            .texture = gfx::TextureView { texture2.AsRef() },
            .src_access_type = gfx::ResourceAccessType::eNone,
            .dst_access_type = gfx::ResourceAccessType::eComputeShaderStorageResourceWrite,
        };
        
        cmd_encoder->ResourceBarrier({}, { texture1_barrier, texture2_barrier });

        graphics_queue->SubmitCommandBuffer({ cmd_encoder->Finish() });
        graphics_queue->WaitIdle();
    }
    stbi_image_free(texture_data);

    struct BlurParams {
        int radius;
        float w[11];
    } blur_params;
    {
        float sigma = 2.5f;
        float two_sigma_sqr = 2.0f * sigma * sigma;
        blur_params.radius = std::min<int>(std::ceil(2.0f * sigma), 5);

        std::fill_n(blur_params.w, 11, 0.0f);
        float sum = 0;
        for (int i = -blur_params.radius; i <= blur_params.radius; i++) {
            float x = i * i;
            blur_params.w[i + blur_params.radius] = std::exp(-x / two_sigma_sqr);
            sum += blur_params.w[i + blur_params.radius];
        }
        for (float &w : blur_params.w) {
            w /= sum;
        }
    }

    gfx::PipelineLayout pipeline_layout {
        .sets_layout = {
            gfx::DescriptorSetLayout {
                .bindings = {
                    gfx::DescriptorSetLayoutBinding {
                        .type = gfx::DescriptorType::eNone,
                    },
                    gfx::DescriptorSetLayoutBinding {
                        .type = gfx::DescriptorType::eSampledTexture,
                        .tex_dim = gfx::TextureViewDimension::e2D,
                        .count = 1,
                    },
                    gfx::DescriptorSetLayoutBinding {
                        .type = gfx::DescriptorType::eRWStorageTexture,
                        .tex_dim = gfx::TextureViewDimension::e2D,
                        .count = 1,
                    }
                }
            }
        },
        .push_constants_size = sizeof(BlurParams),
    };

    std::filesystem::path shader_file = std::filesystem::path(kExamplesDir) / "graphics/compute.hlsl";
    auto shader_compiler = device->GetShaderCompiler();
    auto vert_blur_cs_bytes = shader_compiler->Compile(shader_file, "VertBlurCS", gfx::ShaderStage::eCompute);
    auto vert_blur_cs = device->CreateShaderModule(vert_blur_cs_bytes);
    auto hori_blur_cs_bytes = shader_compiler->Compile(shader_file, "HoriBlurCS", gfx::ShaderStage::eCompute);
    auto hori_blur_cs = device->CreateShaderModule(hori_blur_cs_bytes);

    gfx::ComputePipelineDesc vert_blur_pipeline_desc {
        .name = "vert blur pipeline",
        .layout = pipeline_layout,
        .thread_group = { 1, 256, 1 },
        .compute = vert_blur_cs.AsRef(),
    };
    auto vert_blur_pipeline = device->CreateComputePipeline(vert_blur_pipeline_desc);
    gfx::ComputePipelineDesc hori_blur_pipeline_desc {
        .name = "hori blur pipeline",
        .layout = pipeline_layout,
        .thread_group = { 256, 1, 1 },
        .compute = hori_blur_cs.AsRef(),
    };
    auto hori_blur_pipeline = device->CreateComputePipeline(hori_blur_pipeline_desc);

    gfx::BufferDesc texture_readback_buffer_desc {
        .name = "texture readback buffer",
        .size = texture_data_size,
        .usages = {},
        .memory_property = gfx::BufferMemoryProperty::eGpuToCpu,
    };
    auto texture_readback_buffer = device->CreateBuffer(texture_readback_buffer_desc);

    {
        context->Reset();

        auto cmd_encoder = context->GetCommandEncoder();

        {
            auto compute_encoder = cmd_encoder->BeginComputePass({ "vert blur" });

            compute_encoder->SetPipeline(vert_blur_pipeline);
            compute_encoder->BindShaderParams(0,
                { { std::monostate {}, gfx::TextureView { texture1 }, gfx::TextureView { texture2 } } });
            compute_encoder->PushConstants(blur_params);
            compute_encoder->Dispatch(texture_width, texture_height, 1);
        }
        {
            gfx::TextureBarrier texture1_barrier {
                .texture = gfx::TextureView { texture1.AsRef() },
                .src_access_type = gfx::ResourceAccessType::eComputeShaderSampledTextureRead,
                .dst_access_type = gfx::ResourceAccessType::eComputeShaderStorageResourceWrite,
            };
            gfx::TextureBarrier texture2_barrier {
                .texture = gfx::TextureView { texture2.AsRef() },
                .src_access_type = gfx::ResourceAccessType::eComputeShaderStorageResourceWrite,
                .dst_access_type = gfx::ResourceAccessType::eComputeShaderSampledTextureRead,
            };
            cmd_encoder->ResourceBarrier({}, { texture1_barrier, texture2_barrier });
        }
        {
            auto compute_encoder = cmd_encoder->BeginComputePass({ "hori blur" });

            compute_encoder->SetPipeline(hori_blur_pipeline);
            compute_encoder->BindShaderParams(0,
                { { std::monostate {}, gfx::TextureView { texture2 }, gfx::TextureView { texture1 } } });
            compute_encoder->PushConstants(blur_params);
            compute_encoder->Dispatch(texture_width, texture_height, 1);
        }
        {
            gfx::TextureBarrier texture1_barrier {
                .texture = gfx::TextureView { texture1.AsRef() },
                .src_access_type = gfx::ResourceAccessType::eComputeShaderStorageResourceWrite,
                .dst_access_type = gfx::ResourceAccessType::eTransferRead,
            };
            gfx::BufferBarrier buffer_barrier {
                .buffer = texture_readback_buffer.AsRef(),
                .src_access_type = gfx::ResourceAccessType::eNone,
                .dst_access_type = gfx::ResourceAccessType::eTransferWrite,
            };
            cmd_encoder->ResourceBarrier({ buffer_barrier }, { texture1_barrier });
        }
        {
            gfx::BufferTextureCopyDesc copy_desc {
                .buffer_offset = 0,
                .buffer_bytes_per_row = static_cast<uint32_t>(texture_width * texture_channels * sizeof(uint8_t)),
                .buffer_rows_per_texture = static_cast<uint32_t>(texture_height),
                .texture_offset = {},
                .texture_extent = texture_desc.extent,
                .texture_level = 0,
            };
            cmd_encoder->CopyTextureToBuffer(texture1, texture_readback_buffer, { copy_desc });
        }

        graphics_queue->SubmitCommandBuffer({ cmd_encoder->Finish() });
        graphics_queue->WaitIdle();
    }

    void *mapped_ptr = texture_readback_buffer->Map();
    Vec<uint8_t> out_texture_data(texture_data_size, 0);
    memcpy(out_texture_data.data(), mapped_ptr, texture_data_size);
    texture_readback_buffer->Unmap();

    stbi_write_png("compute.png", texture_width, texture_height, texture_channels, out_texture_data.data(),
        texture_width * texture_channels * sizeof(uint8_t));

    glfwDestroyWindow(window);

    BI_INFO(gGeneralLogger, "Done!");

    return 0;
}
