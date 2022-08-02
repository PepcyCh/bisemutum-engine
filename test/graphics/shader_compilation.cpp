#include <iostream>
#include <fstream>

#include <GLFW/glfw3.h>

#include "core/logger.hpp"
#include "graphics/device.hpp"
#include "graphics/shader_compiler.hpp"

using namespace bismuth;

int main(int argc, char **argv) {
    uint32_t width = 1280;
    uint32_t height = 720;

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(width, height, "Bismuth", nullptr, nullptr);

    gfx::Initialize();

    auto backend = gfx::GraphicsBackend::eVulkan;
    // auto backend = gfx::GraphicsBackend::eD3D12;
    gfx::DeviceDesc device_desc {
        .backend = backend,
        .enable_validation = true,
        .window = window,
    };
    auto device = gfx::Device::Create(device_desc);

    auto shader_compiler = gfx::ShaderCompiler::Create(backend);

    const char *shader_file = "test/graphics/simple_compute.hlsl";
    auto shader_binary_bytes = shader_compiler->Compile(shader_file, "SimpleCompute", gfx::ShaderStage::eCompute);
    // const char *shader_file = "test/graphics/ray_query_compute.hlsl";
    // auto shader_binary_bytes = shader_compiler->Compile(shader_file, "RayQueryCompute", gfx::ShaderStage::eCompute);
    BI_INFO(gGeneralLogger, "Succeeded to compile shader, {} bytes in total", shader_binary_bytes.size());

    {
        std::string shader_binary_file =
            std::string(shader_file) + (backend == gfx::GraphicsBackend::eVulkan ? ".spv" : ".dxil");
        std::ofstream fout(shader_binary_file, std::ios::binary);
        fout.write(reinterpret_cast<char *>(shader_binary_bytes.data()), shader_binary_bytes.size());
    }

    auto shader_module = device->CreateShaderModule(shader_binary_bytes);

    BI_INFO(gGeneralLogger, "Done!");

    return 0;
}
