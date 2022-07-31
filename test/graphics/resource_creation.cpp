#include <iostream>

#include <GLFW/glfw3.h>

#include "core/logger.hpp"
#include "graphics/device.hpp"
#include "graphics/resource.hpp"
#include "graphics/sampler.hpp"

using namespace bismuth;

int main(int argc, char **argv) {
    uint32_t width = 1280;
    uint32_t height = 720;

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(width, height, "Bismuth", nullptr, nullptr);

    gfx::Initialize();

    gfx::DeviceDesc device_desc {
        // .backend = gfx::GraphicsBackend::eVulkan,
        .backend = gfx::GraphicsBackend::eD3D12,
        .enable_validation = true,
        .window = window,
    };
    auto device = gfx::Device::Create(device_desc);

    auto graphics_queue = device->GetQueue(gfx::QueueType::eGraphics);
    auto swap_chain = device->CreateSwapChain(graphics_queue, width, height);

    {
        gfx::BufferDesc buffer_desc {
            .name = "buffer",
            .size = 256,
            .usages = { gfx::BufferUsage::eUnorderedAccess },
            .memory_property = gfx::BufferMemoryProperty::eGpuOnly,
            .persistently_mapped = false,
        };
        auto buffer = device->CreateBuffer(buffer_desc);
    }

    {
        gfx::TextureDesc texture_desc {
            .name = "texture",
            .extent = { 1920, 1080 },
            .levels = 10,
            .format = gfx::ResourceFormat::eRgba32SFloat,
            .usages = { gfx::TextureUsage::eShaderResource }
        };
        auto texture = device->CreateTexture(texture_desc);
    }

    {
        gfx::SamplerDesc sampler_desc {
        };
        auto sampler = device->CreateSampler(sampler_desc);
    }

    BI_INFO(gGeneralLogger, "Done!");

    return 0;
}
