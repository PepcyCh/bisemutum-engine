#include "shader.hpp"

namespace bi::rhi {

ShaderModuleD3D12::ShaderModuleD3D12(Ref<DeviceD3D12> device, ShaderModuleDesc const& desc) : device_(device) {
    shader_bytes_.assign(desc.binary_data.cbegin(), desc.binary_data.cend());
}

auto ShaderModuleD3D12::raw_bytecode() const -> D3D12_SHADER_BYTECODE {
    return D3D12_SHADER_BYTECODE{
        .pShaderBytecode = reinterpret_cast<void const*>(shader_bytes_.data()),
        .BytecodeLength = shader_bytes_.size(),
    };
}

}
