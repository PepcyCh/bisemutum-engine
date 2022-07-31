#pragma once

#include <filesystem>

#include "core/container.hpp"
#include "defines.hpp"
#include "shader.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

namespace fs = std::filesystem;

class ShaderCompiler {
public:
    virtual ~ShaderCompiler() = default;

    static Ptr<ShaderCompiler> Create(GraphicsBackend backend);

    virtual Vec<uint8_t> Compile(const fs::path &src_path, const std::string &entry, ShaderStage stage,
        const HashMap<std::string, std::string> &defines = {}) const = 0;

protected:
    ShaderCompiler() = default;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
