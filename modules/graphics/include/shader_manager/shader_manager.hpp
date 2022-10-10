#pragma once

#include "graphics/shader_compiler.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class Device;

class ShaderManager {
public:
    ShaderManager(Ref<Device> device, const fs::path &binary_dir);

    Ptr<ShaderModule> GetShaderModule(const std::string &src_name, const fs::path &src_path, const std::string &entry,
        ShaderStage stage, const HashMap<std::string, std::string> &defines = {},
        const Vec<fs::path> &include_dirs = {}, bool force = false);

private:
    Ref<Device> device_;
    fs::path binary_dir_;
    Ref<ShaderCompiler> compiler_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
