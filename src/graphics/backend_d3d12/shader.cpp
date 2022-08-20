#include "shader.hpp"

#include <d3d12shader.h>

#include "device.hpp"
#include "dxc_helper.hpp"
#include "graphics/pipeline.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

namespace {

ResourceFormat FromReflectInputFormat(D3D_REGISTER_COMPONENT_TYPE component_type, BYTE mask) {
    int num_components = std::popcount(mask);
    switch (component_type) {
        case D3D_REGISTER_COMPONENT_UNKNOWN: return ResourceFormat::eUndefined;
        case D3D_REGISTER_COMPONENT_UINT32: {
            switch (num_components) {
                case 1: return ResourceFormat::eR32UInt;
                case 2: return ResourceFormat::eRg32UInt;
                case 3: return ResourceFormat::eRgb32UInt;
                case 4: return ResourceFormat::eRgba32UInt;
                default: Unreachable();
            }
        }
        case D3D_REGISTER_COMPONENT_SINT32: {
            switch (num_components) {
                case 1: return ResourceFormat::eR32SInt;
                case 2: return ResourceFormat::eRg32SInt;
                case 3: return ResourceFormat::eRgb32SInt;
                case 4: return ResourceFormat::eRgba32SInt;
                default: Unreachable();
            }
        }
        case D3D_REGISTER_COMPONENT_FLOAT32: {
            switch (num_components) {
                case 1: return ResourceFormat::eR32SFloat;
                case 2: return ResourceFormat::eRg32SFloat;
                case 3: return ResourceFormat::eRgb32SFloat;
                case 4: return ResourceFormat::eRgba32SFloat;
                default: Unreachable();
            }
        }
    }
    Unreachable();
}

VertexSemantics FromReflectSemantics(LPCSTR semantic_name, UINT index) {
    if (strcmp(semantic_name, "POSITION") == 0) {
        return VertexSemantics::ePosition;
    } else if (strcmp(semantic_name, "COLOR") == 0) {
        return VertexSemantics::eColor;
    } else if (strcmp(semantic_name, "NORMAL") == 0) {
        return VertexSemantics::eNormal;
    } else if (strcmp(semantic_name, "TANGENT") == 0) {
        return VertexSemantics::eTangent;
    } else if (strcmp(semantic_name, "BINORMAL") == 0) {
        return VertexSemantics::eBitangent;
    } else if (strcmp(semantic_name, "TEXCOORD") == 0) {
        return static_cast<VertexSemantics>(static_cast<uint8_t>(VertexSemantics::eTexcoord0) + std::min(index, 7u));
    }
    return VertexSemantics::eTexcoord7;
}

D3D12_DESCRIPTOR_RANGE_TYPE FromReflectBindType(D3D_SHADER_INPUT_TYPE type) {
    switch (type) {
        case D3D_SIT_CBUFFER:
            return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        case D3D_SIT_SAMPLER:
            return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        case D3D_SIT_TBUFFER:
        case D3D_SIT_TEXTURE:
        case D3D_SIT_STRUCTURED:
        case D3D_SIT_BYTEADDRESS:
        case D3D_SIT_RTACCELERATIONSTRUCTURE:
            return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        case D3D_SIT_UAV_RWTYPED:
        case D3D_SIT_UAV_RWSTRUCTURED:
        case D3D_SIT_UAV_RWBYTEADDRESS:
        case D3D_SIT_UAV_APPEND_STRUCTURED:
        case D3D_SIT_UAV_CONSUME_STRUCTURED:
        case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
        case D3D_SIT_UAV_FEEDBACKTEXTURE:
            return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    }
    Unreachable();
}

}

#define DXIL_FOURCC(ch0, ch1, ch2, ch3) (                            \
    (uint32_t)(uint8_t)(ch0)        | (uint32_t)(uint8_t)(ch1) << 8  | \
    (uint32_t)(uint8_t)(ch2) << 16  | (uint32_t)(uint8_t)(ch3) << 24   \
)

ShaderModuleD3D12::ShaderModuleD3D12(Ref<DeviceD3D12> device, const Vec<uint8_t> &src_bytes)
    : device_(device), shader_bytes_(src_bytes) {
    /*
    ComPtr<IDxcBlobEncoding> shader_blob;
    DxcHelper::Instance().Utils()->CreateBlob(src_bytes.data(), src_bytes.size(), DXC_CP_ACP, &shader_blob);

    ComPtr<IDxcContainerReflection> container_reflection;
    UINT32 shader_index;
    DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(&container_reflection));
    container_reflection->Load(shader_blob.Get());
    container_reflection->FindFirstPartKind(DXIL_FOURCC('D', 'X', 'I', 'L'), &shader_index);
    ComPtr<ID3D12ShaderReflection> reflection;
    container_reflection->GetPartReflection(shader_index, IID_PPV_ARGS(&reflection));

    D3D12_SHADER_DESC shader_desc;
    reflection->GetDesc(&shader_desc);

    auto shader_stage = static_cast<D3D12_SHADER_VERSION_TYPE>((shader_desc.Version & 0xffff0000) >> 16);

    if (shader_stage == D3D12_SHVER_VERTEX_SHADER) {
        uint8_t max_location = 0;
        Vec<VertexSemantics> semantics(shader_desc.InputParameters);
        Vec<ResourceFormat> formats(shader_desc.InputParameters);
        for (UINT i = 0; i < shader_desc.InputParameters; i++) {
            D3D12_SIGNATURE_PARAMETER_DESC param_desc;
            reflection->GetInputParameterDesc(i, &param_desc);
            semantics[i] = FromReflectSemantics(param_desc.SemanticName, param_desc.SemanticIndex);
            formats[i] = FromReflectInputFormat(param_desc.ComponentType, param_desc.Mask);
            max_location = std::max(max_location, static_cast<uint8_t>(semantics[i]));
        }

        info_.vertex_inputs.resize(max_location, ResourceFormat::eUndefined);
        for (UINT i = 0; i < shader_desc.InputParameters; i++) {
            info_.vertex_inputs[static_cast<size_t>(semantics[i])] = formats[i];
        }
    }

    if (shader_stage == D3D12_SHVER_COMPUTE_SHADER) {
        reflection->GetThreadGroupSize(&info_.compute_local_size_x, &info_.compute_local_size_y,
            &info_.compute_local_size_z);
    }

    Vec<D3D12_SHADER_INPUT_BIND_DESC> bindings(shader_desc.BoundResources);
    info_.bindings.name_map.reserve(shader_desc.BoundResources);
    uint32_t max_set = 0;
    for (UINT i = 0; i < shader_desc.BoundResources; i++) {
        reflection->GetResourceBindingDesc(i, &bindings[i]);
        max_set = std::max(max_set, bindings[i].Space);
    }
    info_.bindings.bindings.resize(max_set);
    Vec<uint32_t> max_binding(max_set, 0);
    for (auto binding : bindings) {
        max_binding[binding.Space] = std::max(max_binding[binding.Space], binding.BindPoint);
    }
    for (uint32_t set = 0; set < max_set; set++) {
        info_.bindings.bindings[set].resize(max_binding[set], D3D12_DESCRIPTOR_RANGE1 { .NumDescriptors = 0 });
    }
    for (auto binding : bindings) {
        info_.bindings.bindings[binding.Space][binding.BindPoint] = D3D12_DESCRIPTOR_RANGE1 {
            .RangeType = FromReflectBindType(binding.Type),
            .NumDescriptors = binding.BindCount,
            .BaseShaderRegister = binding.BindPoint,
            .RegisterSpace = binding.Space,
            .Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
        };
        info_.bindings.name_map[binding.Name] = { binding.Space, binding.BindPoint };
    }
    */
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
