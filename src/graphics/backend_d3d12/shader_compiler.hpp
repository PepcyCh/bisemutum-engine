#pragma once

#include "utils.hpp" // dxcapi needs defination of IUnknown...
#include <dxcapi.h>

#include "graphics/shader_compiler.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class ShaderCompilerD3D12 : public ShaderCompiler {
public:
    ShaderCompilerD3D12();
    ~ShaderCompilerD3D12() override;

    Vec<uint8_t> Compile(const fs::path &src_path, const std::string &entry, ShaderStage stage,
        const HashMap<std::string, std::string> &defines = {}) const override;

private:
    ComPtr<IDxcCompiler3> compiler_;
    ComPtr<IDxcIncludeHandler> include_handler_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
