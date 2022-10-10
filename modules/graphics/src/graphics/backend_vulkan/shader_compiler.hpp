#pragma once

#include "graphics/shader_compiler.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class ShaderCompilerVulkan : public ShaderCompiler {
public:
    ShaderCompilerVulkan();
    ~ShaderCompilerVulkan() override;

    Vec<uint8_t> Compile(const fs::path &src_path, const std::string &entry, ShaderStage stage,
        const HashMap<std::string, std::string> &defines = {}, const Vec<fs::path> &include_dirs = {}) const override;

    const char *BinarySuffix() const override { return ".spv"; }

private:
    Vec<uint32_t> HlslToSpirv(const std::string &src_filename, const std::string &entry, ShaderStage stage,
        const HashMap<std::string, std::string> &defines, const Vec<fs::path> &include_dirs) const;

    std::string SpirvToGlsl(const Vec<uint32_t> &spv) const;

    Vec<uint32_t> GlslToSpirv(const std::string &glsl_src, const std::string &entry, ShaderStage stage) const;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
