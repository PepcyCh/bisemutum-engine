#pragma once

#include <vector>

#include <bisemutum/rhi/shader.hpp>
#include <d3d12.h>

namespace bi::rhi {

struct ShaderModuleD3D12 final : ShaderModule {
    ShaderModuleD3D12(Ref<struct DeviceD3D12> device, ShaderModuleDesc const& desc);

    auto raw_bytecode() const -> D3D12_SHADER_BYTECODE;

private:
    Ref<DeviceD3D12> device_;
    std::vector<std::byte> shader_bytes_;
};

}
