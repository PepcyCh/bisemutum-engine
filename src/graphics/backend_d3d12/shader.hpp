#pragma once

#include "core/container.hpp"
#include "core/ptr.hpp"
#include "utils.hpp"
#include "graphics/shader.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

struct ShaderBindingsD3D12 {
    Vec<Vec<D3D12_DESCRIPTOR_RANGE1>> bindings;
    HashMap<std::string, std::pair<uint8_t, uint16_t>> name_map;

    // uint32_t push_constant_size;

    void Combine(const ShaderBindingsD3D12 &another);
};

struct ShaderInfoD3D12 {
    Vec<ResourceFormat> vertex_inputs;

    uint32_t compute_local_size_x;
    uint32_t compute_local_size_y;
    uint32_t compute_local_size_z;

    ShaderBindingsD3D12 bindings;
};

class ShaderModuleD3D12 : public ShaderModule {
public:
    ShaderModuleD3D12(Ref<class DeviceD3D12> device, const Vec<uint8_t> &src_bytes);
    ~ShaderModuleD3D12() override;

    D3D12_SHADER_BYTECODE RawBytecode() const;

    const ShaderInfoD3D12 &Info() const { return info_; }

private:
    Ref<DeviceD3D12> device_;
    Vec<uint8_t> shader_bytes_;
    ShaderInfoD3D12 info_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
