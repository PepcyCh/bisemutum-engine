#include "shader_manager/shader_manager.hpp"

#include <fstream>
#include <stack>
#include <regex>

#include "graphics/device.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

ShaderManager::ShaderManager(Ref<Device> device, const fs::path &binary_dir)
    : device_(device), binary_dir_(binary_dir), compiler_(device->GetShaderCompiler()) {}

Ptr<ShaderModule> ShaderManager::GetShaderModule(const std::string &src_name, const fs::path &src_path,
    const std::string &entry, ShaderStage stage, const HashMap<std::string, std::string> &defines,
    const Vec<fs::path> &include_dirs, bool force) {
    Vec<std::string> ordered_defines;
    ordered_defines.reserve(defines.size());
    for (const auto &[key, value] : defines) {
        if (value.empty()) {
            ordered_defines.push_back(key);
        } else {
            ordered_defines.push_back(key + "=" + value);
        }
    }
    std::sort(ordered_defines.begin(), ordered_defines.end());
    size_t defines_hash = 0;
    for (const auto &define : ordered_defines) {
        defines_hash = HashCombine(defines_hash, define);
    }
    auto binary_path = binary_dir_ / (src_name + "-" + std::to_string(defines_hash) + compiler_->BinarySuffix());

    if (!fs::exists(binary_path) || fs::last_write_time(src_path) > fs::last_write_time(binary_path) || force) {
        auto bytes = compiler_->Compile(src_path, entry, stage, defines, include_dirs);
        std::ofstream fout(binary_path, std::ios::binary);
        fout.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        return device_->CreateShaderModule(bytes);
    } else {
        std::ifstream fin(binary_path, std::ios::binary);
        fin.seekg(0, std::ios::end);
        size_t length = fin.tellg();
        fin.seekg(0, std::ios::beg);
        Vec<uint8_t> bytes(length);
        fin.read(reinterpret_cast<char *>(bytes.data()), length);
        return device_->CreateShaderModule(bytes);
    }
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
