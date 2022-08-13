#include "pipeline.hpp"

#include "device.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

namespace {

void CreateSignature(const ShaderInfoD3D12 &shader_info, ID3D12Device2 *device, ComPtr<ID3D12RootSignature> &signature) {
    Vec<D3D12_ROOT_PARAMETER1> root_params(shader_info.bindings.bindings.size());
    for (size_t set = 0; set < shader_info.bindings.bindings.size(); set++) {
        const auto &binding_info_raw = shader_info.bindings.bindings[set];
        Vec<D3D12_DESCRIPTOR_RANGE1> binding_info;
        binding_info.reserve(binding_info_raw.size());
        for (const auto &binding : binding_info_raw) {
            if (binding.NumDescriptors != 0) {
                binding_info.push_back(binding);
            }
        }

        root_params[set] = D3D12_ROOT_PARAMETER1 {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable = D3D12_ROOT_DESCRIPTOR_TABLE1 {
                .NumDescriptorRanges = static_cast<UINT>(binding_info.size()),
                .pDescriptorRanges = binding_info.data(),
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        };
    }

    // TODO - push constants (root constants)
    
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC signature_desc {
        .Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
        .Desc_1_1 = D3D12_ROOT_SIGNATURE_DESC1 {
            .NumParameters = static_cast<UINT>(root_params.size()),
            .pParameters = root_params.data(),
            .NumStaticSamplers = 0,
            .pStaticSamplers = nullptr,
            .Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE,
        },
    };
    ComPtr<ID3DBlob> serialized_signature;
    ComPtr<ID3DBlob> error;
    D3D12SerializeVersionedRootSignature(&signature_desc, &serialized_signature, &error);
    device->CreateRootSignature(0, serialized_signature->GetBufferPointer(), serialized_signature->GetBufferSize(),
        IID_PPV_ARGS(&signature));
}

D3D12_BLEND_OP ToDxBlendOp(BlendOp op) {
    switch (op) {
        case BlendOp::eAdd: return D3D12_BLEND_OP_ADD;
        case BlendOp::eSubtract: return D3D12_BLEND_OP_SUBTRACT;
        case BlendOp::eReserveSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
        case BlendOp::eMin: return D3D12_BLEND_OP_MIN;
        case BlendOp::eMax: return D3D12_BLEND_OP_MAX;
    }
    Unreachable();
}

D3D12_BLEND ToDxBlendFactor(BlendFactor factor) {
    switch (factor) {
        case BlendFactor::eZero: return D3D12_BLEND_ZERO;
        case BlendFactor::eOne: return D3D12_BLEND_ONE;
        case BlendFactor::eSrc: return D3D12_BLEND_SRC_COLOR;
        case BlendFactor::eOneMinusSrc: return D3D12_BLEND_INV_SRC_COLOR;
        case BlendFactor::eSrcAlpha: return D3D12_BLEND_SRC_ALPHA;
        case BlendFactor::eOneMinusSrcAlpha: return D3D12_BLEND_INV_SRC_ALPHA;
        case BlendFactor::eDst: return D3D12_BLEND_DEST_COLOR;
        case BlendFactor::eOneMinusDst: return D3D12_BLEND_INV_DEST_COLOR;
        case BlendFactor::eDstAlpha: return D3D12_BLEND_DEST_ALPHA;
        case BlendFactor::eOneMinusDstAlpha: return D3D12_BLEND_INV_DEST_ALPHA;
        case BlendFactor::eSrcAlphaSaturated: return D3D12_BLEND_SRC_ALPHA_SAT;
        case BlendFactor::eConstant: return D3D12_BLEND_BLEND_FACTOR;
        case BlendFactor::eOneMinusConstant: return D3D12_BLEND_INV_BLEND_FACTOR;
    }
    Unreachable();
}

D3D12_FILL_MODE ToDxFillMode(PolygonMode poly) {
    switch (poly) {
        case PolygonMode::eFill: return D3D12_FILL_MODE_SOLID;
        case PolygonMode::eLine: return D3D12_FILL_MODE_WIREFRAME;
        case PolygonMode::ePoint: return D3D12_FILL_MODE_WIREFRAME;
    }
    Unreachable();
}

D3D12_CULL_MODE ToDxCullMode(CullMode cull) {
    switch (cull) {
        case CullMode::eNone: return D3D12_CULL_MODE_NONE;
        case CullMode::eBackFace: return D3D12_CULL_MODE_BACK;
        case CullMode::eFrontFace: return D3D12_CULL_MODE_FRONT;
    }
    Unreachable();
}

D3D12_STENCIL_OP ToDxStencilOp(StencilOp op) {
    switch (op) {
        case StencilOp::eKeep: return D3D12_STENCIL_OP_KEEP;
        case StencilOp::eZero: return D3D12_STENCIL_OP_ZERO;
        case StencilOp::eReplace: return D3D12_STENCIL_OP_REPLACE;
        case StencilOp::eIncrementClamp: return D3D12_STENCIL_OP_INCR_SAT;
        case StencilOp::eDecrementClamp: return D3D12_STENCIL_OP_DECR_SAT;
        case StencilOp::eInvert: return D3D12_STENCIL_OP_INVERT;
        case StencilOp::eIncrementWrap: return D3D12_STENCIL_OP_INCR;
        case StencilOp::eDecrementWrap: return D3D12_STENCIL_OP_DECR;
    }
    Unreachable();
}

LPCSTR ToDxSemanticName(VertexSemantics semantic) {
    switch (semantic) {
        case VertexSemantics::ePosition: return "POSITION";
        case VertexSemantics::eColor: return "COLOR";
        case VertexSemantics::eNormal: return "NORMAL";
        case VertexSemantics::eTangent: return "TANGENT";
        case VertexSemantics::eBitangent: return "BINORMAL";
        case VertexSemantics::eTexcoord0:
        case VertexSemantics::eTexcoord1:
        case VertexSemantics::eTexcoord2:
        case VertexSemantics::eTexcoord3:
        case VertexSemantics::eTexcoord4:
        case VertexSemantics::eTexcoord5:
        case VertexSemantics::eTexcoord6:
        case VertexSemantics::eTexcoord7:
            return "TEXCOORD";
    }
    Unreachable();
}
UINT ToDxSemanticIndex(VertexSemantics semantic) {
    int semantic_raw = static_cast<int>(semantic);
    return std::max(0, semantic_raw - static_cast<int>(VertexSemantics::eTexcoord0));
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE ToDxPrimitiveTopology(PrimitiveTopology topo) {
    switch (topo) {
        case PrimitiveTopology::ePointList:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        case PrimitiveTopology::eLineList:
        case PrimitiveTopology::eLineStrip:
        case PrimitiveTopology::eLineListAdjacency:
        case PrimitiveTopology::eLineStripAdjacency:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case PrimitiveTopology::eTriangleList:
        case PrimitiveTopology::eTriangleStrip:
        case PrimitiveTopology::eTriangleFan:
        case PrimitiveTopology::eTriangleListAdjacency:
        case PrimitiveTopology::eTriangleStripAdjacency:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        case PrimitiveTopology::ePatchList:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
    }
    Unreachable();
}

}

RenderPipelineD3D12::RenderPipelineD3D12(Ref<DeviceD3D12> device, const RenderPipelineDesc &desc)
    : device_(device), desc_(desc) {

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_desc {};

    ShaderInfoD3D12 shader_info {};
    {
        auto shader_vk = desc.shaders.vertex.CastTo<ShaderModuleD3D12>();
        shader_info = shader_vk->Info();
        pipeline_desc.VS = shader_vk->RawBytecode();
    }
    if (desc.shaders.tessellation_control) {
        auto shader_vk = static_cast<ShaderModuleD3D12 *>(desc.shaders.tessellation_control);
        shader_info.bindings.Combine(shader_vk->Info().bindings);
        pipeline_desc.DS = shader_vk->RawBytecode();
    }
    if (desc.shaders.tessellation_evaluation) {
        auto shader_vk = static_cast<ShaderModuleD3D12 *>(desc.shaders.tessellation_evaluation);
        shader_info.bindings.Combine(shader_vk->Info().bindings);
        pipeline_desc.HS = shader_vk->RawBytecode();
    }
    if (desc.shaders.geometry) {
        auto shader_vk = static_cast<ShaderModuleD3D12 *>(desc.shaders.geometry);
        shader_info.bindings.Combine(shader_vk->Info().bindings);
        pipeline_desc.GS = shader_vk->RawBytecode();
    }
    {
        auto shader_vk = desc.shaders.fragment.CastTo<ShaderModuleD3D12>();
        shader_info.bindings.Combine(shader_vk->Info().bindings);
        pipeline_desc.PS = shader_vk->RawBytecode();
    }

    CreateSignature(shader_info, device->Raw(), root_signature_);
    pipeline_desc.pRootSignature = root_signature_.Get();

    pipeline_desc.NumRenderTargets = desc.color_target_state.attachments.size();
    for (size_t i = 0; i < desc.color_target_state.attachments.size(); i++) {
        const auto &attachment = desc.color_target_state.attachments[i];
        pipeline_desc.RTVFormats[i] = ToDxFormat(attachment.format);
        pipeline_desc.BlendState.RenderTarget[i] = D3D12_RENDER_TARGET_BLEND_DESC {
            .BlendEnable = attachment.blend_enable,
            .LogicOpEnable = false,
            .SrcBlend = ToDxBlendFactor(attachment.src_blend_factor),
            .DestBlend = ToDxBlendFactor(attachment.dst_blend_factor),
            .BlendOp = ToDxBlendOp(attachment.blend_op),
            .SrcBlendAlpha = ToDxBlendFactor(attachment.src_alpha_blend_factor),
            .DestBlendAlpha = ToDxBlendFactor(attachment.dst_alpha_blend_factor),
            .BlendOpAlpha = ToDxBlendOp(attachment.alpha_blend_op),
            .LogicOp = D3D12_LOGIC_OP_NOOP,
            .RenderTargetWriteMask = attachment.color_write_mask.RawValue(),
        };
    }

    pipeline_desc.SampleMask = ~0u;

    pipeline_desc.RasterizerState = D3D12_RASTERIZER_DESC {
        .FillMode = ToDxFillMode(desc.primitive_state.polygon_mode),
        .CullMode = ToDxCullMode(desc.primitive_state.cull_mode),
        .FrontCounterClockwise = desc.primitive_state.front_face == FrontFace::eCcw,
        .DepthBias = 0,
        .DepthBiasClamp = 0.0f,
        .SlopeScaledDepthBias = 0.0f,
        .DepthClipEnable = false,
        .MultisampleEnable = false,
        .AntialiasedLineEnable = false,
        .ForcedSampleCount = 0,
        .ConservativeRaster = desc.primitive_state.conservative ? D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON
            : D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
    };

    pipeline_desc.DepthStencilState = D3D12_DEPTH_STENCIL_DESC {
        .DepthEnable = desc.depth_stencil_state.depth_test,
        .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
        .DepthFunc = ToDxCompareFunc(desc.depth_stencil_state.depth_compare_op),
        .StencilEnable = desc.depth_stencil_state.stencil_test,
        .StencilReadMask = desc.depth_stencil_state.stencil_compare_mask,
        .StencilWriteMask = desc.depth_stencil_state.stencil_write_mask,
        .FrontFace = D3D12_DEPTH_STENCILOP_DESC {
            .StencilFailOp = ToDxStencilOp(desc.depth_stencil_state.stencil_front_face.fail_op),
            .StencilDepthFailOp = ToDxStencilOp(desc.depth_stencil_state.stencil_front_face.depth_fail_op),
            .StencilPassOp = ToDxStencilOp(desc.depth_stencil_state.stencil_front_face.pass_op),
            .StencilFunc = ToDxCompareFunc(desc.depth_stencil_state.stencil_front_face.compare_op),
        },
        .BackFace = D3D12_DEPTH_STENCILOP_DESC {
            .StencilFailOp = ToDxStencilOp(desc.depth_stencil_state.stencil_back_face.fail_op),
            .StencilDepthFailOp = ToDxStencilOp(desc.depth_stencil_state.stencil_back_face.depth_fail_op),
            .StencilPassOp = ToDxStencilOp(desc.depth_stencil_state.stencil_back_face.pass_op),
            .StencilFunc = ToDxCompareFunc(desc.depth_stencil_state.stencil_back_face.compare_op),
        },
    };
    pipeline_desc.DSVFormat = ToDxFormat(desc.depth_stencil_state.format);

    Vec<D3D12_INPUT_ELEMENT_DESC> input_elements;
    Vec<size_t> vertex_input_attributes_map(shader_info.vertex_inputs.size(), -1);
    input_elements.reserve(desc.vertex_input_buffers.size());
    for (size_t i = 0; i < shader_info.vertex_inputs.size(); i++) {
        const auto &input_attribute = shader_info.vertex_inputs[i];
        if (input_attribute == ResourceFormat::eUndefined) {
            continue;
        }
        vertex_input_attributes_map[i] = input_elements.size();
        input_elements.push_back(D3D12_INPUT_ELEMENT_DESC {
            .SemanticName = ToDxSemanticName(static_cast<VertexSemantics>(i)),
            .SemanticIndex = ToDxSemanticIndex(static_cast<VertexSemantics>(i)),
            .Format = ToDxFormat(input_attribute),
        });
    }
    for (size_t i = 0; i < desc.vertex_input_buffers.size(); i++) {
        const auto &input_buffer = desc.vertex_input_buffers[i];
        for (const auto &attribute : input_buffer.attributes) {
            size_t input_index = vertex_input_attributes_map[static_cast<size_t>(attribute.semantics)];
            input_elements[input_index].InputSlot = i;
            input_elements[input_index].AlignedByteOffset = attribute.offset;
            input_elements[input_index].InputSlotClass = input_buffer.per_instance
                ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            input_elements[input_index].InstanceDataStepRate = input_buffer.per_instance ? 1 : 0;
        }
    }
    pipeline_desc.InputLayout = D3D12_INPUT_LAYOUT_DESC {
        .pInputElementDescs = input_elements.data(),
        .NumElements = static_cast<UINT>(input_elements.size()),
    };

    pipeline_desc.PrimitiveTopologyType = ToDxPrimitiveTopology(desc.primitive_state.topology);

    pipeline_desc.SampleDesc = DXGI_SAMPLE_DESC { .Count = 1, .Quality = 0 };

    pipeline_desc.NodeMask = 0;

    pipeline_desc.CachedPSO = {};

    pipeline_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    device->Raw()->CreateGraphicsPipelineState(&pipeline_desc, IID_PPV_ARGS(&pipeline_));
}

RenderPipelineD3D12::~RenderPipelineD3D12() {}

ComputePipelineD3D12::ComputePipelineD3D12(Ref<DeviceD3D12> device, const ComputePipelineDesc &desc)
    : device_(device), desc_(desc) {
    auto shader_dx = desc.compute.CastTo<ShaderModuleD3D12>();
    const auto &shader_info = shader_dx->Info();

    local_size_x = shader_info.compute_local_size_x;
    local_size_y = shader_info.compute_local_size_y;
    local_size_z = shader_info.compute_local_size_z;

    CreateSignature(shader_info, device->Raw(), root_signature_);

    D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline_desc {
        .pRootSignature = root_signature_.Get(),
        .CS = shader_dx->RawBytecode(),
        .NodeMask = 0,
        .CachedPSO = {},
        .Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
    };
    device->Raw()->CreateComputePipelineState(&pipeline_desc, IID_PPV_ARGS(&pipeline_));
}

ComputePipelineD3D12::~ComputePipelineD3D12() {}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
