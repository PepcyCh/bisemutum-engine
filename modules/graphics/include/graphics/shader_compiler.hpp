#pragma once

#include <filesystem>

#include "core/container.hpp"
#include "core/ptr.hpp"
#include "defines.hpp"
#include "shader.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

namespace fs = std::filesystem;

class ShaderCompiler {
public:
    virtual ~ShaderCompiler() = default;

    virtual Vec<uint8_t> Compile(const fs::path &src_path, const std::string &entry, ShaderStage stage,
        const HashMap<std::string, std::string> &defines = {}, const Vec<fs::path> &include_dirs = {}) const = 0;

    virtual const char *BinarySuffix() const = 0;

protected:
    ShaderCompiler() = default;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
