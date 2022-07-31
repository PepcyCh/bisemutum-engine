#include "shader_compiler.hpp"

#include "backend_vulkan/shader_compiler.hpp"
#ifdef WIN32
#include "backend_d3d12/shader_compiler.hpp"
#endif

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

Ptr<ShaderCompiler> ShaderCompiler::Create(GraphicsBackend backend) {
    switch (backend) {
        case GraphicsBackend::eVulkan: return Ptr<ShaderCompilerVulkan>::Make();
#ifdef WIN32
        case GraphicsBackend::eD3D12: return Ptr<ShaderCompilerD3D12>::Make();
#else
        case GraphicsBackend::eD3D12: BI_CRTICAL(gGraphicsLogger, "D3D12 backend is only supported on Windows");
#endif
    }
    Unreachable();
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
