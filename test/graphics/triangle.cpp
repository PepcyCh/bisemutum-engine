#include <iostream>

#include <GLFW/glfw3.h>

#include "core/logger.hpp"
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
    auto swap_chain = device->CreateSwapChain(graphics_queue, width, height);

    Ptr<gfx::FrameContext> frames[kNumFrames] = {
        device->CreateFrameContext(),
        device->CreateFrameContext(),
        device->CreateFrameContext(),
    };
    // TODO - semaphore pool & fence pool in FrameContext
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

    const float triangle_vertex_pos[] = {
        -0.5f, -0.5f,
        0.5f, -0.5f,
        0.0f, 0.5f
    };
    const float triangle_vertex_color[] = {
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
    };
    gfx::BufferDesc pos_buffer_desc {
        .name = "pos buffer",
        .size = sizeof(triangle_vertex_pos),
        .usages = { gfx::BufferUsage::eVertex },
        .memory_property = gfx::BufferMemoryProperty::eGpuOnly,
    };
    auto pos_buffer = device->CreateBuffer(pos_buffer_desc);
    gfx::BufferDesc color_buffer_desc {
        .name = "color buffer",
        .size = sizeof(triangle_vertex_color),
        .usages = { gfx::BufferUsage::eVertex },
        .memory_property = gfx::BufferMemoryProperty::eGpuOnly,
    };
    auto color_buffer = device->CreateBuffer(color_buffer_desc);

    {
        frames[0]->Reset();
        auto cmd_encoder = frames[0]->GetCommandEncoder();

        gfx::BufferDesc pos_upload_buffer_desc {
            .name = "pos upload buffer",
            .size = pos_buffer_desc.size,
            .usages = {},
            .memory_property = gfx::BufferMemoryProperty::eCpuToGpu,
        };
        auto pos_upload_buffer = device->CreateBuffer(pos_upload_buffer_desc);
        gfx::BufferDesc color_upload_buffer_desc {
            .name = "color upload buffer",
            .size = color_buffer_desc.size,
            .usages = {},
            .memory_property = gfx::BufferMemoryProperty::eCpuToGpu,
        };
        auto color_upload_buffer = device->CreateBuffer(color_upload_buffer_desc);

        gfx::BufferBarrier pos_upload_buffer_barrier {
            .buffer = pos_upload_buffer.AsRef(),
            .src_access_type = gfx::ResourceAccessType::eNone,
            .src_access_stage = gfx::ResourceAccessStage::eNone,
            .dst_access_type = gfx::ResourceAccessType::eTransferRead,
            .dst_access_stage = gfx::ResourceAccessStage::eTransfer,
        };
        gfx::BufferBarrier color_upload_buffer_barrier {
            .buffer = color_upload_buffer.AsRef(),
            .src_access_type = gfx::ResourceAccessType::eNone,
            .src_access_stage = gfx::ResourceAccessStage::eNone,
            .dst_access_type = gfx::ResourceAccessType::eTransferRead,
            .dst_access_stage = gfx::ResourceAccessStage::eTransfer,
        };
        gfx::BufferBarrier pos_buffer_barrier {
            .buffer = pos_buffer.AsRef(),
            .src_access_type = gfx::ResourceAccessType::eNone,
            .src_access_stage = gfx::ResourceAccessStage::eNone,
            .dst_access_type = gfx::ResourceAccessType::eTransferWrite,
            .dst_access_stage = gfx::ResourceAccessStage::eTransfer,
        };
        gfx::BufferBarrier color_buffer_barrier {
            .buffer = color_buffer.AsRef(),
            .src_access_type = gfx::ResourceAccessType::eNone,
            .src_access_stage = gfx::ResourceAccessStage::eNone,
            .dst_access_type = gfx::ResourceAccessType::eTransferWrite,
            .dst_access_stage = gfx::ResourceAccessStage::eTransfer,
        };
        cmd_encoder->ResourceBarrier(
            { pos_upload_buffer_barrier, color_upload_buffer_barrier, pos_buffer_barrier, color_buffer_barrier },
            {}
        );

        void *pos_mapped_ptr = pos_upload_buffer->Map();
        memcpy(pos_mapped_ptr, triangle_vertex_pos, sizeof(triangle_vertex_pos));
        pos_upload_buffer->Unmap();
        cmd_encoder->CopyBufferToBuffer(pos_upload_buffer.AsRef(), pos_buffer.AsRef());

        void *color_mapped_ptr = color_upload_buffer->Map();
        memcpy(color_mapped_ptr, triangle_vertex_color, sizeof(triangle_vertex_color));
        color_upload_buffer->Unmap();
        cmd_encoder->CopyBufferToBuffer(color_upload_buffer.AsRef(), color_buffer.AsRef());

        pos_buffer_barrier = gfx::BufferBarrier {
            .buffer = pos_buffer.AsRef(),
            .src_access_type = gfx::ResourceAccessType::eTransferWrite,
            .src_access_stage = gfx::ResourceAccessStage::eTransfer,
            .dst_access_type = gfx::ResourceAccessType::eVertexBufferRead,
            .dst_access_stage = gfx::ResourceAccessStage::eInputAssembler,
        };
        color_buffer_barrier = gfx::BufferBarrier {
            .buffer = color_buffer.AsRef(),
            .src_access_type = gfx::ResourceAccessType::eTransferWrite,
            .src_access_stage = gfx::ResourceAccessStage::eTransfer,
            .dst_access_type = gfx::ResourceAccessType::eVertexBufferRead,
            .dst_access_stage = gfx::ResourceAccessStage::eInputAssembler,
        };
        cmd_encoder->ResourceBarrier({ pos_buffer_barrier, color_buffer_barrier }, {});

        graphics_queue->SubmitCommandBuffer({ cmd_encoder->Finish() });
        graphics_queue->WaitIdle();
    }

    std::filesystem::path shader_file = "../../../../test/graphics/triangle.hlsl";
    auto shader_compiler = gfx::ShaderCompiler::Create(backend);
    auto vs_bytes = shader_compiler->Compile(shader_file, "VS", gfx::ShaderStage::eVertex);
    auto vs_sm = device->CreateShaderModule(vs_bytes);
    auto fs_bytes = shader_compiler->Compile(shader_file, "FS", gfx::ShaderStage::eFragment);
    auto fs_sm = device->CreateShaderModule(fs_bytes);

    gfx::RenderPipelineDesc pipeline_desc {
        .name = "triangle pipeline",
        .layout = {},
        .vertex_input_buffers = {
            gfx::VertexInputBufferDesc {
                .stride = 2 * sizeof(float),
                .per_instance = false,
                .attributes = { { 0, gfx::VertexSemantics::ePosition, gfx::ResourceFormat::eRg32SFloat } },
            },
            gfx::VertexInputBufferDesc {
                .stride = 3 * sizeof(float),
                .per_instance = false,
                .attributes = { { 0, gfx::VertexSemantics::eColor, gfx::ResourceFormat::eRgb32SFloat } },
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
            .src_access_stage = gfx::ResourceAccessStage::eNone,
            .dst_access_type = gfx::ResourceAccessType::eRenderAttachmentWrite,
            .dst_access_stage = gfx::ResourceAccessStage::eColorAttachment,
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
            auto render_encoder = cmd_encoder->BeginRenderPass({ "Triangle" }, rt_desc);

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
            render_encoder->BindVertexBuffer({ { pos_buffer }, { color_buffer } });
            render_encoder->Draw(3);
        }

        gfx::TextureBarrier color_target_to_present {
            .texture = back_buffer,
            .src_access_type = gfx::ResourceAccessType::eRenderAttachmentWrite,
            .src_access_stage = gfx::ResourceAccessStage::eColorAttachment,
            .dst_access_type = gfx::ResourceAccessType::ePresent,
            .dst_access_stage = gfx::ResourceAccessStage::ePresent,
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
