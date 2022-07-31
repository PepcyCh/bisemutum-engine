#pragma once

#include "graphics/shader_compiler.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class ShaderCompilerVulkan : public ShaderCompiler {
public:
    ShaderCompilerVulkan();
    ~ShaderCompilerVulkan() override;

    Vec<uint8_t> Compile(const fs::path &src_path, const std::string &entry, ShaderStage stage,
        const HashMap<std::string, std::string> &defines = {}) const override;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
