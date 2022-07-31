#include "shader.hpp"

#include <d3d12shader.h>

#include "device.hpp"
#include "dxc_helper.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

#define DXIL_FOURCC(ch0, ch1, ch2, ch3) (                            \
    (uint32_t)(uint8_t)(ch0)        | (uint32_t)(uint8_t)(ch1) << 8  | \
    (uint32_t)(uint8_t)(ch2) << 16  | (uint32_t)(uint8_t)(ch3) << 24   \
)

ShaderModuleD3D12::ShaderModuleD3D12(Ref<DeviceD3D12> device, const Vec<uint8_t> &src_bytes)
    : device_(device), shader_bytes_(src_bytes) {
    ComPtr<IDxcBlobEncoding> shader_blob;
    DxcHelper::Instance().Utils()->CreateBlob(src_bytes.data(), src_bytes.size(), DXC_CP_ACP, &shader_blob);

    ComPtr<IDxcContainerReflection> container_reflection;
    UINT32 shader_index;
    DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(&container_reflection));
    container_reflection->Load(shader_blob.Get());
    container_reflection->FindFirstPartKind(DXIL_FOURCC('D', 'X', 'I', 'L'), &shader_index);
    ComPtr<ID3D12ShaderReflection> reflection;
    container_reflection->GetPartReflection(shader_index, IID_PPV_ARGS(&reflection));

    // TODO - reflection
}

ShaderModuleD3D12::~ShaderModuleD3D12() {}

D3D12_SHADER_BYTECODE ShaderModuleD3D12::RawBytecode() const {
    return D3D12_SHADER_BYTECODE {
        .pShaderBytecode = reinterpret_cast<const void *>(shader_bytes_.data()),
        .BytecodeLength = shader_bytes_.size(),
    };
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
