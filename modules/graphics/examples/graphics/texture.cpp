#include <iostream>

#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "core/logger.hpp"
#include "graphics/examples.hpp"
#include "graphics/device.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/shader_compiler.hpp"

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

    int width = 1280;
    int height = 720;

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(width, height, "Bismuth", nullptr, nullptr);

    gfx::Initialize();

    gfx::DeviceDesc device_desc {
        .backend = backend,
        .enable_validation = true,
        .window = window,
    };
    auto device = gfx::Device::Create(device_desc);

    auto graphics_queue = device->GetQueue(gfx::QueueType::eGraphics);
    gfx::SwapChainDesc swap_chain_desc {
        .queue = graphics_queue,
        .width = static_cast<uint32_t>(width),
        .height = static_cast<uint32_t>(height),
    };
    auto swap_chain = device->CreateSwapChain(swap_chain_desc);

    Ptr<gfx::FrameContext> frames[kNumFrames] = {
        device->CreateFrameContext(),
        device->CreateFrameContext(),
        device->CreateFrameContext(),
    };
    Ptr<gfx::Semaphore> acquire_semaphores[kNumFrames] = {
        device->CreateSemaphore(),
        device->CreateSemaphore(),
        device->CreateSemaphore(),
    };
    Ptr<gfx::Semaphore> signal_semaphores[kNumFrames] = {
        device->CreateSemaphore(),
        device->CreateSemaphore(),
        device->CreateSemaphore(),
    };
    Ptr<gfx::Fence> fences[kNumFrames] = {
        device->CreateFence(),
        device->CreateFence(),
        device->CreateFence(),
    };

    const float vertex_data[] = {
        -0.5f, -0.5f,   0.0f, 0.0f,
        0.5f, -0.5f,    1.0f, 0.0f,
        0.5f, 0.5f,     1.0f, 1.0f,
        -0.5f, 0.5f,    0.0f, 1.0f,
    };
    gfx::BufferDesc vertex_buffer_desc {
        .name = "vertex buffer",
        .size = sizeof(vertex_data),
        .usages = { gfx::BufferUsage::eVertex },
        .memory_property = gfx::BufferMemoryProperty::eGpuOnly,
    };
    auto vertex_buffer = device->CreateBuffer(vertex_buffer_desc);
    const uint16_t index_data[] = { 0, 1, 2, 0, 2, 3 };
    gfx::BufferDesc index_buffer_desc {
        .name = "index buffer",
        .size = sizeof(index_data),
        .usages = { gfx::BufferUsage::eIndex },
        .memory_property = gfx::BufferMemoryProperty::eGpuOnly,
    };
    auto index_buffer = device->CreateBuffer(index_buffer_desc);

    {
        frames[0]->Reset();
        auto cmd_encoder = frames[0]->GetCommandEncoder();

        gfx::BufferDesc vertex_upload_buffer_desc {
            .name = "vertex upload buffer",
            .size = vertex_buffer_desc.size,
            .usages = {},
            .memory_property = gfx::BufferMemoryProperty::eCpuToGpu,
        };
        auto vertex_upload_buffer = device->CreateBuffer(vertex_upload_buffer_desc);
        gfx::BufferDesc index_upload_buffer_desc {
            .name = "index upload buffer",
            .size = index_buffer_desc.size,
            .usages = {},
            .memory_property = gfx::BufferMemoryProperty::eCpuToGpu,
        };
        auto index_upload_buffer = device->CreateBuffer(index_upload_buffer_desc);

        gfx::BufferBarrier vertex_upload_buffer_barrier {
            .buffer = vertex_upload_buffer.AsRef(),
            .src_access_type = gfx::ResourceAccessType::eNone,
            .dst_access_type = gfx::ResourceAccessType::eTransferRead,
        };
        gfx::BufferBarrier index_upload_buffer_barrier {
            .buffer = index_upload_buffer.AsRef(),
            .src_access_type = gfx::ResourceAccessType::eNone,
            .dst_access_type = gfx::ResourceAccessType::eTransferRead,
        };
        gfx::BufferBarrier vertex_buffer_barrier {
            .buffer = vertex_buffer.AsRef(),
            .src_access_type = gfx::ResourceAccessType::eNone,
            .dst_access_type = gfx::ResourceAccessType::eTransferWrite,
        };
        gfx::BufferBarrier index_buffer_barrier {
            .buffer = index_buffer.AsRef(),
            .src_access_type = gfx::ResourceAccessType::eNone,
            .dst_access_type = gfx::ResourceAccessType::eTransferWrite,
        };
        cmd_encoder->ResourceBarrier(
            { vertex_upload_buffer_barrier, index_upload_buffer_barrier, vertex_buffer_barrier, index_buffer_barrier },
            {}
        );

        void *vertex_mapped_ptr = vertex_upload_buffer->Map();
        memcpy(vertex_mapped_ptr, vertex_data, sizeof(vertex_data));
        vertex_upload_buffer->Unmap();
        cmd_encoder->CopyBufferToBuffer(vertex_upload_buffer.AsRef(), vertex_buffer.AsRef());

        void *index_mapped_ptr = index_upload_buffer->Map();
        memcpy(index_mapped_ptr, index_data, sizeof(index_data));
        index_upload_buffer->Unmap();
        cmd_encoder->CopyBufferToBuffer(index_upload_buffer.AsRef(), index_buffer.AsRef());

        vertex_buffer_barrier = gfx::BufferBarrier {
            .buffer = vertex_buffer.AsRef(),
            .src_access_type = gfx::ResourceAccessType::eTransferWrite,
            .dst_access_type = gfx::ResourceAccessType::eVertexBufferRead,
        };
        index_buffer_barrier = gfx::BufferBarrier {
            .buffer = index_buffer.AsRef(),
            .src_access_type = gfx::ResourceAccessType::eTransferWrite,
            .dst_access_type = gfx::ResourceAccessType::eIndexBufferRead,
        };
        cmd_encoder->ResourceBarrier({ vertex_buffer_barrier, index_buffer_barrier }, {});

        graphics_queue->SubmitCommandBuffer({ cmd_encoder->Finish() });
        graphics_queue->WaitIdle();
    }

    int texture_width, texture_height, texture_channels;
    auto texture_path = std::filesystem::path(kExamplesDir) / "graphics/texture.png";
    uint8_t *texture_data = stbi_load(texture_path.string().c_str(), &texture_width, &texture_height,
        &texture_channels, 0);
    gfx::ResourceFormat texture_format =
        texture_channels == 3 ? gfx::ResourceFormat::eRgb8Srgb : gfx::ResourceFormat::eRgba8Srgb;
    gfx::TextureDesc texture_desc {
        .name = "texture",
        .extent = { static_cast<uint32_t>(texture_width), static_cast<uint32_t>(texture_height) },
        .levels = 1,
        .format = texture_format,
        .dim = gfx::TextureDimension::e2D,
        .usages = { gfx::TextureUsage::eSampled },
    };
    auto texture = device->CreateTexture(texture_desc);
    {
        frames[0]->Reset();
        auto cmd_encoder = frames[0]->GetCommandEncoder();

        size_t texture_data_size = texture_width * texture_height * texture_channels * sizeof(uint8_t);
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
        gfx::TextureBarrier texture_barrier {
            .texture = gfx::TextureView { texture.AsRef() },
            .src_access_type = gfx::ResourceAccessType::eNone,
            .dst_access_type = gfx::ResourceAccessType::eTransferWrite,
        };
        
        cmd_encoder->ResourceBarrier({ buffer_barrier }, { texture_barrier });

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
        cmd_encoder->CopyBufferToTexture(texture_upload_buffer.AsRef(), texture.AsRef(), { copy_desc });

        texture_barrier = gfx::TextureBarrier {
            .texture = gfx::TextureView { texture.AsRef() },
            .src_access_type = gfx::ResourceAccessType::eTransferWrite,
            .dst_access_type = gfx::ResourceAccessType::eRenderShaderSampledTextureRead,
        };
        
        cmd_encoder->ResourceBarrier({}, { texture_barrier });

        graphics_queue->SubmitCommandBuffer({ cmd_encoder->Finish() });
        graphics_queue->WaitIdle();
    }
    stbi_image_free(texture_data);

    gfx::SamplerDesc sampler_desc {
        .mag_filter = gfx::SamplerFilterMode::eNearest,
        .min_filter = gfx::SamplerFilterMode::eNearest,
        .mipmap_mode = gfx::SamplerMipmapMode::eNearest,
        .address_mode_u = gfx::SamplerAddressMode::eRepeat,
        .address_mode_v = gfx::SamplerAddressMode::eRepeat,
        .address_mode_w = gfx::SamplerAddressMode::eRepeat,
    };
    auto sampler = device->CreateSampler(sampler_desc);

    std::filesystem::path shader_file = std::filesystem::path(kExamplesDir) / "graphics/texture.hlsl";
    auto shader_compiler = device->GetShaderCompiler();
    auto vs_bytes = shader_compiler->Compile(shader_file, "VS", gfx::ShaderStage::eVertex);
    auto vs_sm = device->CreateShaderModule(vs_bytes);
    auto fs_bytes = shader_compiler->Compile(shader_file, "FS", gfx::ShaderStage::eFragment);
    auto fs_sm = device->CreateShaderModule(fs_bytes);

    gfx::PipelineLayout pipeline_layout {
        .sets_layout = {
            gfx::DescriptorSetLayout {
                .bindings = {
                    gfx::DescriptorSetLayoutBinding {
                        .type = gfx::DescriptorType::eSampledTexture,
                        .tex_dim = gfx::TextureViewDimension::e2D,
                        .count = 1,
                    }
                }
            },
            gfx::DescriptorSetLayout {
                .bindings = {
                    gfx::DescriptorSetLayoutBinding {
                        .type = gfx::DescriptorType::eSampler,
                        .count = 1,
                        .immutable_samplers = { sampler.AsRef() },
                    }
                }
            }
        },
        .push_constants_size = 0,
    };
    gfx::RenderPipelineDesc pipeline_desc {
        .name = "texture pipeline",
        .layout = pipeline_layout,
        .vertex_input_buffers = {
            gfx::VertexInputBufferDesc {
                .stride = 4 * sizeof(float),
                .per_instance = false,
                .attributes = {
                    { 0, gfx::VertexSemantics::ePosition, gfx::ResourceFormat::eRg32SFloat },
                    { 2 * sizeof(float), gfx::VertexSemantics::eTexcoord0, gfx::ResourceFormat::eRg32SFloat },
                },
            }
        },
        .primitive_state = gfx::PrimitiveState {},
        .depth_stencil_state = gfx::DepthStencilState {},
        .color_target_state = gfx::ColorTargetState {
            .attachments = { {} }
        },
        .shaders = {
            .vertex = vs_sm.AsRef(),
            .fragment = fs_sm.AsRef(),
        }
    };
    auto pipeline = device->CreateRenderPipeline(pipeline_desc);

    uint32_t curr_frame = 0;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int new_width, new_height;
        glfwGetFramebufferSize(window, &new_width, &new_height);
        if (new_width != width || new_height != height) {
            graphics_queue->WaitIdle();
            width = new_width;
            height = new_height;
            swap_chain->Resize(width, height);
        }

        fences[curr_frame]->Wait();

        frames[curr_frame]->Reset();

        if (!swap_chain->AcquireNextTexture(acquire_semaphores[curr_frame].AsRef())) {
            BI_WARN(gGeneralLogger, "Failed to acquire swap chain texture");
            break;
        }

        auto back_buffer = gfx::TextureView { swap_chain->GetCurrentTexture() };
        
        auto cmd_encoder = frames[curr_frame]->GetCommandEncoder();

        gfx::TextureBarrier present_to_color_target {
            .texture = back_buffer,
            .src_access_type = gfx::ResourceAccessType::eNone,
            .dst_access_type = gfx::ResourceAccessType::eColorAttachmentWrite,
        };
        cmd_encoder->ResourceBarrier({}, { present_to_color_target });

        {
            gfx::RenderTargetDesc rt_desc {
                .colors = {
                    gfx::ColorAttachmentDesc {
                        .texture = back_buffer,
                        .clear_color = { 0.2f, 0.3f, 0.5f, 1.0f },
                        .clear = true,
                        .store = true,
                    },
                },
            };
            auto render_encoder = cmd_encoder->BeginRenderPass({ "Texture" }, rt_desc);

            gfx::Viewport viewport {
                .width = static_cast<float>(width),
                .height = static_cast<float>(height),
            };
            render_encoder->SetViewports({ viewport });
            gfx::Scissor scissor {
                .width = static_cast<uint32_t>(width),
                .height = static_cast<uint32_t>(height),
            };
            render_encoder->SetScissors({ scissor });

            render_encoder->SetPipeline(pipeline.AsRef());
            render_encoder->BindShaderParams(0, { { gfx::TextureView { texture.AsRef() } } });
            render_encoder->BindShaderParams(1, { { std::monostate {} } });
            render_encoder->BindVertexBuffer({ { vertex_buffer } });
            render_encoder->BindIndexBuffer(index_buffer, 0, gfx::IndexType::eUInt16);
            render_encoder->DrawIndexed(6);
        }

        gfx::TextureBarrier color_target_to_present {
            .texture = back_buffer,
            .src_access_type = gfx::ResourceAccessType::eColorAttachmentWrite,
            .dst_access_type = gfx::ResourceAccessType::ePresent,
        };
        cmd_encoder->ResourceBarrier({}, { color_target_to_present });

        graphics_queue->SubmitCommandBuffer({ cmd_encoder->Finish() },
            { acquire_semaphores[curr_frame] }, { signal_semaphores[curr_frame] },
            fences[curr_frame].Get());

        swap_chain->Present({ signal_semaphores[curr_frame] });

        curr_frame = (curr_frame + 1) % kNumFrames;
    }

    graphics_queue->WaitIdle();

    glfwDestroyWindow(window);

    BI_INFO(gGeneralLogger, "Done!");

    return 0;
}
