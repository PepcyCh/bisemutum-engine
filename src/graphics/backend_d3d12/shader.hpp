#pragma once

#include "core/container.hpp"
#include "core/ptr.hpp"
#include "utils.hpp"
#include "graphics/shader.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class ShaderModuleD3D12 : public ShaderModule {
public:
    ShaderModuleD3D12(Ref<class DeviceD3D12> device, const Vec<uint8_t> &src_bytes);
    ~ShaderModuleD3D12() override;

    D3D12_SHADER_BYTECODE RawBytecode() const;

private:
    Ref<DeviceD3D12> device_;
    Vec<uint8_t> shader_bytes_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
