#include <iostream>

#include <GLFW/glfw3.h>

#include "core/logger.hpp"
#include "graphics/device.hpp"
#include "graphics/resource.hpp"
#include "graphics/sampler.hpp"

using namespace bismuth;

int main(int argc, char **argv) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(1280, 720, "Bismuth", nullptr, nullptr);

    gfx::Initialize();

    gfx::DeviceDesc device_desc {
        .backend = gfx::DeviceBackend::eVulkan,
        // .backend = gfx::DeviceBackend::eD3D12,
        .enable_validation = true,
        .window = window,
    };
    auto device = gfx::Device::CreateDevice(device_desc);

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
