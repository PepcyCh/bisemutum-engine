#include <iostream>

#include <GLFW/glfw3.h>

#include <core/module_manager.hpp>
#include <graphics/examples.hpp>
#include <graphics/device.hpp>
#include <graphics/pipeline.hpp>
#include <graphics/shader_compiler.hpp>
#include <render_graph/graph.hpp>

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

    ModuleManager::Load<gfx::GraphicsModule>();

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

    auto rg = gfx::RenderGraph(device, graphics_queue, kNumFrames);

    auto init_context = device->CreateFrameContext();
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
        init_context->Reset();
        auto cmd_encoder = init_context->GetCommandEncoder();

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

    auto shader_compiler = device->GetShaderCompiler();

    std::filesystem::path gentex_shader = std::filesystem::path(kExamplesDir) / "render_graph/gentex.hlsl";
    auto gentex_cs_bytes = shader_compiler->Compile(gentex_shader, "CS", gfx::ShaderStage::eCompute);
    auto gentex_cs_sm = device->CreateShaderModule(gentex_cs_bytes);

    std::filesystem::path showtex_shader = std::filesystem::path(kExamplesDir) / "render_graph/showtex.hlsl";
    auto showtex_vs_bytes = shader_compiler->Compile(showtex_shader, "VS", gfx::ShaderStage::eVertex);
    auto showtex_vs_sm = device->CreateShaderModule(showtex_vs_bytes);
    auto showtex_fs_bytes = shader_compiler->Compile(showtex_shader, "FS", gfx::ShaderStage::eFragment);
    auto showtex_fs_sm = device->CreateShaderModule(showtex_fs_bytes);

    gfx::PipelineLayout gentex_pipeline_layout {
        .sets_layout = {
            gfx::DescriptorSetLayout {
                .bindings = {
                    gfx::DescriptorSetLayoutBinding { .type = gfx::DescriptorType::eNone },
                    gfx::DescriptorSetLayoutBinding {
                        .type = gfx::DescriptorType::eRWStorageTexture,
                        .tex_dim = gfx::TextureViewDimension::e2D,
                        .count = 1,
                    }
                },
            },
        },
        .push_constants_size = 2 * sizeof(uint32_t),
    };
    gfx::ComputePipelineDesc gentex_pipeline_desc {
        .name = "gentex pipeline",
        .layout = gentex_pipeline_layout,
        .thread_group = { 16, 16, 1 },
        .compute = gentex_cs_sm.AsRef(),
    };
    auto gentex_pipeline = device->CreateComputePipeline(gentex_pipeline_desc);

    gfx::SamplerDesc sampler_desc {
        .mag_filter = gfx::SamplerFilterMode::eLinear,
        .min_filter = gfx::SamplerFilterMode::eLinear,
        .mipmap_mode = gfx::SamplerMipmapMode::eLinear,
        .address_mode_u = gfx::SamplerAddressMode::eRepeat,
        .address_mode_v = gfx::SamplerAddressMode::eRepeat,
        .address_mode_w = gfx::SamplerAddressMode::eRepeat,
    };
    auto sampler = device->CreateSampler(sampler_desc);
    gfx::PipelineLayout showtex_pipeline_layout {
        .sets_layout = {
            gfx::DescriptorSetLayout {
                .bindings = {
                    gfx::DescriptorSetLayoutBinding {
                        .type = gfx::DescriptorType::eSampledTexture,
                        .tex_dim = gfx::TextureViewDimension::e2D,
                        .count = 1,
                    }
                },
            },
            gfx::DescriptorSetLayout {
                .bindings = {
                    gfx::DescriptorSetLayoutBinding {
                        .type = gfx::DescriptorType::eSampler,
                        .count = 1,
                        .immutable_samplers = { sampler.AsRef() },
                    }
                }
            },
        },
        .push_constants_size = 0,
    };
    gfx::RenderPipelineDesc showtex_pipeline_desc {
        .name = "showtex pipeline",
        .layout = showtex_pipeline_layout,
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
            .vertex = showtex_vs_sm.AsRef(),
            .fragment = showtex_fs_sm.AsRef(),
        }
    };
    auto showtex_pipeline = device->CreateRenderPipeline(showtex_pipeline_desc);

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

        if (!swap_chain->AcquireNextTexture(acquire_semaphores[curr_frame].AsRef())) {
            BI_WARN(gGeneralLogger, "Failed to acquire swap chain texture");
            break;
        }

        auto back_buffer = rg.ImportTexture("back buffer", swap_chain->GetCurrentTexture());
        const uint32_t texture_width = 512;
        const uint32_t texture_height = 512;
        auto texture = rg.AddTexture("texture",
            [&](gfx::TextureBuilder &builder) {
                builder
                    .Dim2D(gfx::ResourceFormat::eRgba8UNorm, texture_width, texture_height)
                    .Mipmap()
                    .Usage({ gfx::TextureUsage::eRWStorage, gfx::TextureUsage::eSampled });
            }
        );
        rg.AddComputePass("gentex",
            [&](gfx::ComputePassBuilder &builder) {
                builder.Write("output", texture, true);
            },
            [&](Ref<gfx::ComputeCommandEncoder> compute_encoder, const gfx::PassResource &resources) {
                compute_encoder->SetPipeline(gentex_pipeline.AsRef());
                uint32_t texture_size[2] = { texture_width, texture_height };
                compute_encoder->PushConstants(texture_size, 2 * sizeof(uint32_t));
                compute_encoder->BindShaderParams(0, { {
                    std::monostate {},
                    gfx::TextureView { resources.Texture("output") },
                } });
                compute_encoder->Dispatch(texture_width, texture_height, 1);
            }
        );
        rg.AddRenderPass("showtex",
            [&](gfx::RenderPassBuilder &builder) {
                builder
                    .Color(0, gfx::RenderPassColorTargetBuilder(back_buffer).ClearColor(0.2f, 0.3f, 0.5f))
                    .Read("texture", texture);
            },
            [&](Ref<gfx::RenderCommandEncoder> render_encoder, const gfx::PassResource &resources) {
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

                render_encoder->SetPipeline(showtex_pipeline.AsRef());
                render_encoder->BindShaderParams(0, { { gfx::TextureView { resources.Texture("texture") } } });
                render_encoder->BindShaderParams(1, { { std::monostate {} } });
                render_encoder->BindVertexBuffer({ { vertex_buffer } });
                render_encoder->BindIndexBuffer(index_buffer, 0, gfx::IndexType::eUInt16);
                render_encoder->DrawIndexed(6);
            }
        );
        rg.AddPresentPass(back_buffer);

        rg.Compile();
        rg.Execute({ acquire_semaphores[curr_frame] }, { signal_semaphores[curr_frame] }, fences[curr_frame].Get());

        swap_chain->Present({ signal_semaphores[curr_frame] });

        curr_frame = (curr_frame + 1) % kNumFrames;
    }

    graphics_queue->WaitIdle();

    glfwDestroyWindow(window);

    BI_INFO(gGeneralLogger, "Done!");

    return 0;
}
