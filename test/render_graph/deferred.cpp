#include <iostream>

#include <GLFW/glfw3.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/logger.hpp"
#include "graphics/device.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/shader_compiler.hpp"
#include "render_graph/graph.hpp"

using namespace bismuth;

static constexpr uint32_t kNumFrames = 3;

static constexpr float kCubeVertices[] = {
    // bottom
    -1.0f, -1.0f, -1.0f,     0.0f, -1.0f,  0.0f,
     1.0f, -1.0f, -1.0f,     0.0f, -1.0f,  0.0f,
     1.0f, -1.0f,  1.0f,     0.0f, -1.0f,  0.0f,
    -1.0f, -1.0f,  1.0f,     0.0f, -1.0f,  0.0f,
    // top
    -1.0f,  1.0f, -1.0f,     0.0f,  1.0f,  0.0f,
    -1.0f,  1.0f,  1.0f,     0.0f,  1.0f,  0.0f,
     1.0f,  1.0f,  1.0f,     0.0f,  1.0f,  0.0f,
     1.0f,  1.0f, -1.0f,     0.0f,  1.0f,  0.0f,
    // -X
    -1.0f, -1.0f, -1.0f,    -1.0f,  0.0f,  0.0f,
    -1.0f, -1.0f,  1.0f,    -1.0f,  0.0f,  0.0f,
    -1.0f,  1.0f,  1.0f,    -1.0f,  0.0f,  0.0f,
    -1.0f,  1.0f, -1.0f,    -1.0f,  0.0f,  0.0f,
    // +X
     1.0f, -1.0f, -1.0f,     1.0f,  0.0f,  0.0f,
     1.0f,  1.0f, -1.0f,     1.0f,  0.0f,  0.0f,
     1.0f,  1.0f,  1.0f,     1.0f,  0.0f,  0.0f,
     1.0f, -1.0f,  1.0f,     1.0f,  0.0f,  0.0f,
    // -Z
    -1.0f, -1.0f, -1.0f,     0.0f,  0.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,     0.0f,  0.0f, -1.0f,
     1.0f,  1.0f, -1.0f,     0.0f,  0.0f, -1.0f,
     1.0f, -1.0f, -1.0f,     0.0f,  0.0f, -1.0f,
    // +Z
    -1.0f, -1.0f,  1.0f,     0.0f,  0.0f,  1.0f,
     1.0f, -1.0f,  1.0f,     0.0f,  0.0f,  1.0f,
     1.0f,  1.0f,  1.0f,     0.0f,  0.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,     0.0f,  0.0f,  1.0f,
};
static constexpr uint16_t kCubeIndices[] = {
    0, 1, 2, 0, 2, 3,
    4, 5, 6, 4, 6, 7,
    8, 9, 10, 8, 10, 11,
    12, 13, 14, 12, 14, 15,
    16, 17, 18, 16, 18, 19,
    20, 21, 22, 20, 22, 23,
};

Ptr<gfx::Buffer> CreateGpuBufferWithData(Ref<gfx::Device> device, Ref<gfx::CommandEncoder> cmd_encoder,
    const std::string &name, const void *data, size_t size, BitFlags<gfx::BufferUsage> usage,
    BitFlags<gfx::ResourceAccessType> access, Vec<Ptr<gfx::Buffer>> &upload_buffers) {
    gfx::BufferDesc gpu_buffer_desc {
        .name = name.c_str(),
        .size = size,
        .usages = usage,
        .memory_property = gfx::BufferMemoryProperty::eGpuOnly,
    };
    auto gpu_buffer = device->CreateBuffer(gpu_buffer_desc);

    gfx::BufferDesc upload_buffer_desc {
        .name = (name + " upload").c_str(),
        .size = size,
        .usages = {},
        .memory_property = gfx::BufferMemoryProperty::eCpuToGpu,
    };
    auto upload_buffer = device->CreateBuffer(upload_buffer_desc);

    gfx::BufferBarrier gpu_buffer_barrier {
        .buffer = gpu_buffer.AsRef(),
        .src_access_type = gfx::ResourceAccessType::eNone,
        .dst_access_type = gfx::ResourceAccessType::eTransferWrite,
    };
    gfx::BufferBarrier upload_buffer_barrier {
        .buffer = upload_buffer.AsRef(),
        .src_access_type = gfx::ResourceAccessType::eNone,
        .dst_access_type = gfx::ResourceAccessType::eTransferRead,
    };
    cmd_encoder->ResourceBarrier({ gpu_buffer_barrier, upload_buffer_barrier }, {});

    void *mapped_ptr = upload_buffer->Map();
    memcpy(mapped_ptr, data, size);
    upload_buffer->Unmap();
    cmd_encoder->CopyBufferToBuffer(upload_buffer.AsRef(), gpu_buffer.AsRef());

    gpu_buffer_barrier = gfx::BufferBarrier {
        .buffer = gpu_buffer.AsRef(),
        .src_access_type = gfx::ResourceAccessType::eTransferWrite,
        .dst_access_type = access,
    };
    cmd_encoder->ResourceBarrier({ gpu_buffer_barrier }, {});

    upload_buffers.emplace_back(std::move(upload_buffer));

    return gpu_buffer;
}

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

    auto rg = gfxrg::RenderGraph(device, graphics_queue, kNumFrames);

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


    Vec<Ptr<gfx::Buffer>> upload_buffers;
    auto init_context = device->CreateFrameContext();
    init_context->Reset();
    auto init_cmd_encoder = init_context->GetCommandEncoder();

    auto vertex_buffer = CreateGpuBufferWithData(device, init_cmd_encoder,
        "vertex buffer", kCubeVertices, sizeof(kCubeVertices),
        gfx::BufferUsage::eVertex, gfx::ResourceAccessType::eVertexBufferRead, upload_buffers);
    auto index_buffer = CreateGpuBufferWithData(device, init_cmd_encoder,
        "index buffer", kCubeIndices, sizeof(kCubeIndices),
        gfx::BufferUsage::eIndex, gfx::ResourceAccessType::eIndexBufferRead, upload_buffers);

    struct Camera {
        glm::mat4 proj_view;
    };
    struct InstanceData {
        glm::mat4 model;
        glm::mat4 model_it;
        glm::vec4 color;
    };
    struct PointLight {
        glm::vec3 position;
        float strength;
        glm::vec3 color;
        float _padding;
    };

    const uint32_t num_instances = 9;
    Camera camera;
    const glm::vec3 camera_pos(0.0f, 5.0f, 9.0f);
    const glm::vec3 camera_lookat(0.0f);
    const float camera_fov = 45.0f;
    const float camera_near = 0.01f;
    const float camera_far = 100.0f;
    {
        glm::mat4 view = glm::lookAt(camera_pos, camera_lookat, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 proj = glm::perspective(glm::radians(camera_fov), static_cast<float>(width) / height,
            camera_near, camera_far);
        proj[1] *= -1.0f;
        camera.proj_view = proj * view;
    }
    gfx::BufferDesc camera_buffer_desc {
        .name = "camera",
        .size = sizeof(Camera),
        .usages = gfx::BufferUsage::eUniform,
        .memory_property = gfx::BufferMemoryProperty::eCpuToGpu,
    };
    auto camera_buffer = device->CreateBuffer(camera_buffer_desc);
    {
        gfx::BufferBarrier camera_buffer_barrier {
            .buffer = camera_buffer.AsRef(),
            .src_access_type = gfx::ResourceAccessType::eNone,
            .dst_access_type = gfx::ResourceAccessType::eRenderShaderUniformBufferRead,
        };
        init_cmd_encoder->ResourceBarrier({ camera_buffer_barrier }, {});

        void *mapped_ptr = camera_buffer->Map();
        memcpy(mapped_ptr, &camera, sizeof(camera));
        camera_buffer->Unmap();
    }
    InstanceData instance_data[num_instances];
    {
        for (int i = -1; i <= 1; i++) {
            for (int j = -1; j <= 1; j++) {
                int index = (i + 1) * 3 + j + 1;
                glm::mat4 model(1.0f);
                model = glm::translate(model, glm::vec3(i * 3.0f, 0.0f, j * 3.0f));
                instance_data[index].model = model;
                instance_data[index].model_it = glm::inverse(glm::transpose(model));
                instance_data[index].color = glm::vec4((i + 1) * 0.5f, 0.5f, (j + 1) * 0.5f, 1.0f);
            }
        }
    }
    auto instance_data_buffer = CreateGpuBufferWithData(device, init_cmd_encoder,
        "instance data", instance_data, sizeof(instance_data),
        gfx::BufferUsage::eStorage, gfx::ResourceAccessType::eRenderShaderStorageResourceRead, upload_buffers);
    PointLight point_light {
        .position = { 0.0f, 5.0f, 0.0f },
        .strength = 1.5f,
        .color = { 1.0f, 0.95f, 0.8f },
    };
    auto point_light_buffer = CreateGpuBufferWithData(device, init_cmd_encoder,
        "point light", &point_light, sizeof(point_light),
        gfx::BufferUsage::eUniform, gfx::ResourceAccessType::eRenderShaderUniformBufferRead, upload_buffers);

    graphics_queue->SubmitCommandBuffer({ init_cmd_encoder->Finish() });
    graphics_queue->WaitIdle();
    upload_buffers.clear();


    auto shader_compiler = gfx::ShaderCompiler::Create(backend);

    std::filesystem::path gbuffer_shader = "../../../../test/render_graph/deferred_gbuffer.hlsl";
    auto gbuffer_vs_bytes = shader_compiler->Compile(gbuffer_shader, "VS", gfx::ShaderStage::eVertex);
    auto gbuffer_vs_sm = device->CreateShaderModule(gbuffer_vs_bytes);
    auto gbuffer_fs_bytes = shader_compiler->Compile(gbuffer_shader, "FS", gfx::ShaderStage::eFragment);
    auto gbuffer_fs_sm = device->CreateShaderModule(gbuffer_fs_bytes);

    std::filesystem::path lighting_shader = "../../../../test/render_graph/deferred_lighting.hlsl";
    auto lighting_vs_bytes = shader_compiler->Compile(lighting_shader, "VS", gfx::ShaderStage::eVertex);
    auto lighting_vs_sm = device->CreateShaderModule(lighting_vs_bytes);
    auto lighting_fs_bytes = shader_compiler->Compile(lighting_shader, "FS", gfx::ShaderStage::eFragment);
    auto lighting_fs_sm = device->CreateShaderModule(lighting_fs_bytes);

    gfx::PipelineLayout gbuffer_pipeline_layout {
        .sets_layout = {
            gfx::DescriptorSetLayout {
                .bindings = {
                    gfx::DescriptorSetLayoutBinding { .type = gfx::DescriptorType::eNone },
                    gfx::DescriptorSetLayoutBinding {
                        .type = gfx::DescriptorType::eUniformBuffer,
                        .count = 1,
                        .struct_stride = sizeof(Camera),
                    },
                    gfx::DescriptorSetLayoutBinding {
                        .type = gfx::DescriptorType::eStorageBuffer,
                        .count = 1,
                        .struct_stride = sizeof(InstanceData),
                    },
                }
            },
        },
        .push_constants_size = 0,
    };
    gfx::RenderPipelineDesc gbuffer_pipeline_desc {
        .name = "gbuffer pipeline",
        .layout = gbuffer_pipeline_layout,
        .vertex_input_buffers = {
            gfx::VertexInputBufferDesc {
                .stride = 6 * sizeof(float),
                .per_instance = false,
                .attributes = {
                    { 0, gfx::VertexSemantics::ePosition, gfx::ResourceFormat::eRgb32SFloat },
                    { 3 * sizeof(float), gfx::VertexSemantics::eNormal, gfx::ResourceFormat::eRgb32SFloat },
                },
            }
        },
        .primitive_state = gfx::PrimitiveState {},
        .depth_stencil_state = gfx::DepthStencilState {},
        .color_target_state = gfx::ColorTargetState {
            .attachments = { {}, {}, {} }
        },
        .shaders = {
            .vertex = gbuffer_vs_sm.AsRef(),
            .fragment = gbuffer_fs_sm.AsRef(),
        }
    };
    auto gbuffer_pipeline = device->CreateRenderPipeline(gbuffer_pipeline_desc);

    gfx::PipelineLayout lighting_pipeline_layout {
        .sets_layout = {
            gfx::DescriptorSetLayout {
                .bindings = {
                    gfx::DescriptorSetLayoutBinding { .type = gfx::DescriptorType::eNone },
                    gfx::DescriptorSetLayoutBinding {
                        .type = gfx::DescriptorType::eSampledTexture,
                        .tex_dim = gfx::TextureViewDimension::e2D,
                        .count = 1,
                    },
                    gfx::DescriptorSetLayoutBinding {
                        .type = gfx::DescriptorType::eSampledTexture,
                        .tex_dim = gfx::TextureViewDimension::e2D,
                        .count = 1,
                    },
                    gfx::DescriptorSetLayoutBinding {
                        .type = gfx::DescriptorType::eSampledTexture,
                        .tex_dim = gfx::TextureViewDimension::e2D,
                        .count = 1,
                    },
                },
            },
            gfx::DescriptorSetLayout {
                .bindings = {
                    gfx::DescriptorSetLayoutBinding {
                        .type = gfx::DescriptorType::eUniformBuffer,
                        .count = 1,
                        .struct_stride = sizeof(PointLight),
                    },
                }
            },
        },
        .push_constants_size = 0,
    };
    gfx::RenderPipelineDesc lighting_pipeline_desc {
        .name = "lighting pipeline",
        .layout = lighting_pipeline_layout,
        .vertex_input_buffers = {},
        .primitive_state = gfx::PrimitiveState {},
        .depth_stencil_state = gfx::DepthStencilState {},
        .color_target_state = gfx::ColorTargetState {
            .attachments = { {} }
        },
        .shaders = {
            .vertex = lighting_vs_sm.AsRef(),
            .fragment = lighting_fs_sm.AsRef(),
        }
    };
    auto lighting_pipeline = device->CreateRenderPipeline(lighting_pipeline_desc);

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

            glm::mat4 view = glm::lookAt(camera_pos, camera_lookat, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 proj = glm::perspective(glm::radians(camera_fov), static_cast<float>(width) / height,
                camera_near, camera_far);
            proj[1] *= -1.0f;
            camera.proj_view = proj * view;
            void *mapped_ptr = camera_buffer->Map();
            memcpy(mapped_ptr, &camera, sizeof(camera));
            camera_buffer->Unmap();
        }

        fences[curr_frame]->Wait();

        if (!swap_chain->AcquireNextTexture(acquire_semaphores[curr_frame].AsRef())) {
            BI_WARN(gGeneralLogger, "Failed to acquire swap chain texture");
            break;
        }

        auto back_buffer = rg.ImportTexture("back buffer", swap_chain->GetCurrentTexture());
        auto gbuffer_pos = rg.AddTexture("gbuffer position",
            [&](gfxrg::TextureBuilder &builder) {
                builder
                    .Dim2D(gfx::ResourceFormat::eRgba32SFloat, width, height)
                    .Usage({ gfx::TextureUsage::eColorAttachment, gfx::TextureUsage::eSampled });
            }
        );
        auto gbuffer_normal = rg.AddTexture("gbuffer normal",
            [&](gfxrg::TextureBuilder &builder) {
                builder
                    .Dim2D(gfx::ResourceFormat::eRgba32SFloat, width, height)
                    .Usage({ gfx::TextureUsage::eColorAttachment, gfx::TextureUsage::eSampled });
            }
        );
        auto gbuffer_color = rg.AddTexture("gbuffer color",
            [&](gfxrg::TextureBuilder &builder) {
                builder
                    .Dim2D(gfx::ResourceFormat::eRgba8UNorm, width, height)
                    .Usage({ gfx::TextureUsage::eColorAttachment, gfx::TextureUsage::eSampled });
            }
        );
        auto gbuffer_depth = rg.AddTexture("gbuffer depth",
            [&](gfxrg::TextureBuilder &builder) {
                builder
                    .Dim2D(gfx::ResourceFormat::eD32SFloat, width, height)
                    .Usage({ gfx::TextureUsage::eDepthStencilAttachment });
            }
        );
        rg.AddRenderPass("gbuffer",
            [&](gfxrg::RenderPassBuilder &builder) {
                builder
                    .Color(0, gfxrg::RenderPassColorTargetBuilder(gbuffer_pos).ClearColor())
                    .Color(1, gfxrg::RenderPassColorTargetBuilder(gbuffer_normal).ClearColor())
                    .Color(2, gfxrg::RenderPassColorTargetBuilder(gbuffer_color).ClearColor())
                    .DepthStencil(gfxrg::RenderPassDepthStencilTargetBuilder(gbuffer_depth).ClearDepthStencil());
            },
            [&](Ref<gfx::RenderCommandEncoder> render_encoder, const gfxrg::PassResource &resources) {
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

                render_encoder->SetPipeline(gbuffer_pipeline.AsRef());
                render_encoder->BindShaderParams(0, { {
                    std::monostate {},
                    gfx::BufferRange { camera_buffer.AsRef() },
                    gfx::BufferRange { instance_data_buffer.AsRef() },
                } });
                render_encoder->BindVertexBuffer({ { vertex_buffer } });
                render_encoder->BindIndexBuffer(index_buffer, 0, gfx::IndexType::eUInt16);
                render_encoder->DrawIndexed(sizeof(kCubeIndices) / sizeof(uint16_t), num_instances);
            }
        );
        rg.AddRenderPass("lighting",
            [&](gfxrg::RenderPassBuilder &builder) {
                builder
                    .Color(0, gfxrg::RenderPassColorTargetBuilder(back_buffer).ClearColor())
                    .Read("gbuffer pos", gbuffer_pos)
                    .Read("gbuffer normal", gbuffer_normal)
                    .Read("gbuffer color", gbuffer_color);
            },
            [&](Ref<gfx::RenderCommandEncoder> render_encoder, const gfxrg::PassResource &resources) {
                render_encoder->SetPipeline(lighting_pipeline.AsRef());
                render_encoder->BindShaderParams(0, { {
                    std::monostate {},
                    gfx::TextureView { resources.Texture("gbuffer pos") },
                    gfx::TextureView { resources.Texture("gbuffer normal") },
                    gfx::TextureView { resources.Texture("gbuffer color") },
                } });
                render_encoder->BindShaderParams(1, { { gfx::BufferRange { point_light_buffer.AsRef() } } });
                render_encoder->Draw(3);
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
