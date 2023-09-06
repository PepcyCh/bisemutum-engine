#include "pipeline.hpp"

#include <algorithm>

#include <bisemutum/prelude/misc.hpp>
#include <bisemutum/utils/crypto.hpp>
#include <fmt/format.h>

#include "device.hpp"
#include "shader.hpp"
#include "sampler.hpp"
#include "utils.hpp"

namespace bi::rhi {

namespace {

auto to_dx_descriptor_range_type(DescriptorType type) -> D3D12_DESCRIPTOR_RANGE_TYPE {
    switch (type) {
        case DescriptorType::sampler:
            return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        case DescriptorType::uniform_buffer:
            return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        case DescriptorType::read_only_storage_buffer:
        case DescriptorType::sampled_texture:
        case DescriptorType::read_only_storage_texture:
        case DescriptorType::acceleration_structure:
            return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        case DescriptorType::read_write_storage_buffer:
        case DescriptorType::read_write_storage_texture:
            return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        default: unreachable();
    }
}

auto to_dx_shader_visibility(BitFlags<ShaderStage> stage) -> D3D12_SHADER_VISIBILITY {
    if (stage == ShaderStage::vertex) {
        return D3D12_SHADER_VISIBILITY_VERTEX;
    } else if (stage == ShaderStage::tessellation_control) {
        return D3D12_SHADER_VISIBILITY_HULL;
    } else if (stage == ShaderStage::tessellation_evaluation) {
        return D3D12_SHADER_VISIBILITY_DOMAIN;
    } else if (stage == ShaderStage::geometry) {
        return D3D12_SHADER_VISIBILITY_GEOMETRY;
    } else if (stage == ShaderStage::fragment) {
        return D3D12_SHADER_VISIBILITY_PIXEL;
    } else if (stage == ShaderStage::task) {
        return D3D12_SHADER_VISIBILITY_AMPLIFICATION;
    } else if (stage == ShaderStage::mesh) {
        return D3D12_SHADER_VISIBILITY_MESH;
    } else {
        return D3D12_SHADER_VISIBILITY_ALL;
    }
}

auto create_pipeline_layout(
    Ref<DeviceD3D12> device,
    Microsoft::WRL::ComPtr<ID3D12RootSignature> &root_signature,
    std::vector<BindGroupLayout> const& bind_groups_layout,
    std::vector<StaticSampler> const& static_samplers,
    PushConstantsDesc const& push_constants_desc,
    bool allow_input_assembler
) -> void {
    std::vector<D3D12_ROOT_PARAMETER1> root_params(bind_groups_layout.size() + (push_constants_desc.size > 0 ? 1 : 0));
    std::vector<D3D12_STATIC_SAMPLER_DESC> static_samplers_dx(static_samplers.size());

    size_t num_bindings = 0;
    for (auto const& group : bind_groups_layout) {
        num_bindings += group.size();
    }
    std::vector<D3D12_DESCRIPTOR_RANGE1> bindings_info(num_bindings);
    auto p_binding_info = bindings_info.data();

    for (size_t group_ind = 0; auto const& group : bind_groups_layout) {
        auto p_binding_info_start = p_binding_info;
        BitFlags<ShaderStage> visibility{};
        for (auto const& entry : group) {
            *p_binding_info = D3D12_DESCRIPTOR_RANGE1{
                .RangeType = to_dx_descriptor_range_type(entry.type),
                .NumDescriptors = entry.count,
                .BaseShaderRegister = entry.binding_or_register,
                .RegisterSpace = entry.space,
                .Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
                .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
            };
            ++p_binding_info;
            visibility.set(entry.visibility);
        }
        root_params[group_ind] = D3D12_ROOT_PARAMETER1{
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable = D3D12_ROOT_DESCRIPTOR_TABLE1{
                .NumDescriptorRanges = static_cast<uint32_t>(group.size()),
                .pDescriptorRanges = p_binding_info_start,
            },
            .ShaderVisibility = to_dx_shader_visibility(visibility),
        };
        ++group_ind;
    }

    for (size_t i = 0; i < static_samplers.size(); i++) {
        auto sampler = static_samplers[i].sampler.cast_to<const SamplerD3D12>();
        static_samplers_dx[i].Filter = sampler->sampler_desc().Filter;
        static_samplers_dx[i].AddressU = sampler->sampler_desc().AddressU;
        static_samplers_dx[i].AddressV = sampler->sampler_desc().AddressV;
        static_samplers_dx[i].AddressW = sampler->sampler_desc().AddressW;
        static_samplers_dx[i].MipLODBias = sampler->sampler_desc().MipLODBias;
        static_samplers_dx[i].MaxAnisotropy = sampler->sampler_desc().MaxAnisotropy;
        static_samplers_dx[i].ComparisonFunc = sampler->sampler_desc().ComparisonFunc;
        static_samplers_dx[i].BorderColor = sampler->static_border_color();
        static_samplers_dx[i].MinLOD = sampler->sampler_desc().MinLOD;
        static_samplers_dx[i].MaxLOD = sampler->sampler_desc().MaxLOD;
        static_samplers_dx[i].ShaderRegister = static_samplers[i].binding_or_register;
        static_samplers_dx[i].RegisterSpace = static_samplers[i].space;
        static_samplers_dx[i].ShaderVisibility = to_dx_shader_visibility(static_samplers[i].visibility);
    }

    if (push_constants_desc.size > 0) {
        root_params.back() = D3D12_ROOT_PARAMETER1{
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
            .Constants = D3D12_ROOT_CONSTANTS{
                .ShaderRegister = push_constants_desc.register_,
                .RegisterSpace = push_constants_desc.space,
                .Num32BitValues = (push_constants_desc.size + 3) / 4,
            },
            .ShaderVisibility = to_dx_shader_visibility(push_constants_desc.visibility),
        };
    }

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC signature_desc{
        .Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
        .Desc_1_1 = D3D12_ROOT_SIGNATURE_DESC1 {
            .NumParameters = static_cast<UINT>(root_params.size()),
            .pParameters = root_params.data(),
            .NumStaticSamplers = static_cast<UINT>(static_samplers_dx.size()),
            .pStaticSamplers = static_samplers_dx.data(),
            .Flags = allow_input_assembler
                ? D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT : D3D12_ROOT_SIGNATURE_FLAG_NONE,
        },
    };
    Microsoft::WRL::ComPtr<ID3DBlob> serialized_signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    D3D12SerializeVersionedRootSignature(&signature_desc, &serialized_signature, &error);
    device->raw()->CreateRootSignature(
        0, serialized_signature->GetBufferPointer(), serialized_signature->GetBufferSize(),
        IID_PPV_ARGS(&root_signature)
    );
}

auto to_dx_blend_op(BlendOp op) -> D3D12_BLEND_OP {
    switch (op) {
        case BlendOp::add: return D3D12_BLEND_OP_ADD;
        case BlendOp::subtract: return D3D12_BLEND_OP_SUBTRACT;
        case BlendOp::reverse_subtrace: return D3D12_BLEND_OP_REV_SUBTRACT;
        case BlendOp::min: return D3D12_BLEND_OP_MIN;
        case BlendOp::max: return D3D12_BLEND_OP_MAX;
        default: unreachable();
    }
}

auto to_dx_blend_factor(BlendFactor factor) -> D3D12_BLEND {
    switch (factor) {
        case BlendFactor::zero: return D3D12_BLEND_ZERO;
        case BlendFactor::one: return D3D12_BLEND_ONE;
        case BlendFactor::src: return D3D12_BLEND_SRC_COLOR;
        case BlendFactor::one_minus_src: return D3D12_BLEND_INV_SRC_COLOR;
        case BlendFactor::src_alpha: return D3D12_BLEND_SRC_ALPHA;
        case BlendFactor::one_minus_src_alpha: return D3D12_BLEND_INV_SRC_ALPHA;
        case BlendFactor::dst: return D3D12_BLEND_DEST_COLOR;
        case BlendFactor::one_minus_dst: return D3D12_BLEND_INV_DEST_COLOR;
        case BlendFactor::dst_alpha: return D3D12_BLEND_DEST_ALPHA;
        case BlendFactor::one_minus_dst_alpha: return D3D12_BLEND_INV_DEST_ALPHA;
        case BlendFactor::src_alpha_saturated: return D3D12_BLEND_SRC_ALPHA_SAT;
        case BlendFactor::constant: return D3D12_BLEND_BLEND_FACTOR;
        case BlendFactor::one_minus_constant: return D3D12_BLEND_INV_BLEND_FACTOR;
        default: unreachable();
    }
}

auto to_dx_fill_mode(PolygonMode poly) -> D3D12_FILL_MODE {
    switch (poly) {
        case PolygonMode::fill: return D3D12_FILL_MODE_SOLID;
        case PolygonMode::line: return D3D12_FILL_MODE_WIREFRAME;
        case PolygonMode::point: return D3D12_FILL_MODE_WIREFRAME;
        default: unreachable();
    }
}

auto to_dx_cull_mode(CullMode cull) -> D3D12_CULL_MODE {
    switch (cull) {
        case CullMode::none: return D3D12_CULL_MODE_NONE;
        case CullMode::back_face: return D3D12_CULL_MODE_BACK;
        case CullMode::front_face: return D3D12_CULL_MODE_FRONT;
        default: unreachable();
    }
}

auto to_dx_stencil_op(StencilOp op) -> D3D12_STENCIL_OP {
    switch (op) {
        case StencilOp::keep: return D3D12_STENCIL_OP_KEEP;
        case StencilOp::zero: return D3D12_STENCIL_OP_ZERO;
        case StencilOp::replace: return D3D12_STENCIL_OP_REPLACE;
        case StencilOp::increment_clamp: return D3D12_STENCIL_OP_INCR_SAT;
        case StencilOp::decrement_clamp: return D3D12_STENCIL_OP_DECR_SAT;
        case StencilOp::invert: return D3D12_STENCIL_OP_INVERT;
        case StencilOp::increment_wrap: return D3D12_STENCIL_OP_INCR;
        case StencilOp::decrement_wrap: return D3D12_STENCIL_OP_DECR;
        default: unreachable();
    }
}

auto to_dx_semantic_name(VertexSemantics semantic) -> LPCSTR {
    switch (semantic) {
        case VertexSemantics::position: return "POSITION";
        case VertexSemantics::color: return "COLOR";
        case VertexSemantics::normal: return "NORMAL";
        case VertexSemantics::tangent: return "TANGENT";
        case VertexSemantics::bitangent: return "BINORMAL";
        case VertexSemantics::texcoord0:
        case VertexSemantics::texcoord1:
        case VertexSemantics::texcoord2:
        case VertexSemantics::texcoord3:
        case VertexSemantics::texcoord4:
        case VertexSemantics::texcoord5:
        case VertexSemantics::texcoord6:
        case VertexSemantics::texcoord7:
            return "TEXCOORD";
        default: unreachable();
    }
}
auto to_dx_semantic_index(VertexSemantics semantic) -> UINT {
    int semantic_raw = static_cast<int>(semantic);
    return std::max(0, semantic_raw - static_cast<int>(VertexSemantics::texcoord0));
}

auto to_dx_primitive_topology_type(PrimitiveTopology topo) -> D3D12_PRIMITIVE_TOPOLOGY_TYPE {
    switch (topo) {
        case PrimitiveTopology::point_list:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        case PrimitiveTopology::line_list:
        case PrimitiveTopology::line_strip:
        case PrimitiveTopology::line_list_adjacency:
        case PrimitiveTopology::line_strip_adjacency:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case PrimitiveTopology::triangle_list:
        case PrimitiveTopology::triangle_strip:
        case PrimitiveTopology::triangle_list_adjacency:
        case PrimitiveTopology::triangle_strip_adjacency:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        case PrimitiveTopology::patch_list:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
        default: unreachable();
    }
}

auto to_dx_primitive_topology(PrimitiveTopology topo) -> D3D12_PRIMITIVE_TOPOLOGY {
    switch (topo) {
        case PrimitiveTopology::point_list: return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        case PrimitiveTopology::line_list: return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        case PrimitiveTopology::line_strip: return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case PrimitiveTopology::line_list_adjacency: return D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
        case PrimitiveTopology::line_strip_adjacency: return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;
        case PrimitiveTopology::triangle_list: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case PrimitiveTopology::triangle_strip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        case PrimitiveTopology::triangle_list_adjacency: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
        case PrimitiveTopology::triangle_strip_adjacency: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
        // TODO - support tessellation
        case PrimitiveTopology::patch_list: return D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST;
        default: unreachable();
    }
}

auto hash_graphics_pipeline_desc(D3D12_GRAPHICS_PIPELINE_STATE_DESC const& desc) -> std::string {
    auto config_hash = hash(
        ByteHash<D3D12_RASTERIZER_DESC>{}(desc.RasterizerState),
        ByteHash<D3D12_DEPTH_STENCIL_DESC>{}(desc.DepthStencilState),
        ByteHash<D3D12_BLEND_DESC>{}(desc.BlendState),
        ByteHash<DXGI_SAMPLE_DESC>{}(desc.SampleDesc),
        desc.NumRenderTargets,
        desc.DSVFormat,
        desc.PrimitiveTopologyType,
        desc.InputLayout.NumElements,
        desc.Flags
    );
    for (uint32_t i = 0; i < desc.NumRenderTargets; i++) {
        config_hash = hash_combine(config_hash, desc.RTVFormats[i]);
    }
    for (uint32_t i = 0; i < desc.InputLayout.NumElements; i++) {
        auto const& v = desc.InputLayout.pInputElementDescs[i];
        config_hash = hash_combine(
            config_hash,
            hash(
                v.SemanticName, v.SemanticIndex, v.Format, v.InputSlot, v.InputSlotClass,
                v.AlignedByteOffset, v.InstanceDataStepRate
            )
        );
    }

    auto vs_md5 = crypto::md5({reinterpret_cast<std::byte const*>(desc.VS.pShaderBytecode), desc.VS.BytecodeLength});
    auto hs_md5 = crypto::md5({reinterpret_cast<std::byte const*>(desc.HS.pShaderBytecode), desc.HS.BytecodeLength});
    auto ds_md5 = crypto::md5({reinterpret_cast<std::byte const*>(desc.DS.pShaderBytecode), desc.DS.BytecodeLength});
    auto gs_md5 = crypto::md5({reinterpret_cast<std::byte const*>(desc.GS.pShaderBytecode), desc.GS.BytecodeLength});
    auto ps_md5 = crypto::md5({reinterpret_cast<std::byte const*>(desc.PS.pShaderBytecode), desc.PS.BytecodeLength});

    return fmt::format(
        "GRAPHICS-{:x}-{}-{}-{}-{}-{}",
        config_hash, vs_md5.as_string(), hs_md5.as_string(), ds_md5.as_string(), gs_md5.as_string(), ps_md5.as_string()
    );
}

auto hash_compute_pipeline_desc(D3D12_COMPUTE_PIPELINE_STATE_DESC const& desc) -> std::string {
    auto config_hash = hash(desc.Flags);

    auto cs_md5 = crypto::md5({reinterpret_cast<std::byte const*>(desc.CS.pShaderBytecode), desc.CS.BytecodeLength});

    return fmt::format(
        "COMPUTE-{:x}-{}",
        config_hash, cs_md5.as_string()
    );
}

auto chars_to_wstring(char const* chars, size_t length) -> std::wstring {
    auto size = MultiByteToWideChar(CP_UTF8, 0, chars, length, nullptr, 0);
    auto temp  = new wchar_t[size];
    MultiByteToWideChar(CP_UTF8, 0, chars, length, temp, size);
    std::wstring str(temp, temp + size - 1);
    delete[] temp;
    return str;
}

} // namespace

GraphicsPipelineD3D12::GraphicsPipelineD3D12(Ref<DeviceD3D12> device, GraphicsPipelineDesc const& desc)
    : GraphicsPipeline(desc), device_(device)
{
    create_pipeline_layout(
        device_, root_signature_,
        desc_.bind_groups_layout, desc_.static_samplers, desc_.push_constants, true
    );
    root_constant_index_ = desc_.bind_groups_layout.size();

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_desc{};

    {
        auto shader_dx = desc_.shaders.vertex.shader_module.cast_to<const ShaderModuleD3D12>();
        pipeline_desc.VS = shader_dx->raw_bytecode();
    }
    if (desc_.shaders.tessellation_control) {
        auto shader_dx = desc_.shaders.tessellation_control.value().shader_module.cast_to<const ShaderModuleD3D12>();
        pipeline_desc.DS = shader_dx->raw_bytecode();
    }
    if (desc_.shaders.tessellation_evaluation) {
        auto shader_dx = desc_.shaders.tessellation_evaluation.value().shader_module.cast_to<const ShaderModuleD3D12>();
        pipeline_desc.HS = shader_dx->raw_bytecode();
    }
    if (desc_.shaders.geometry) {
        auto shader_dx = desc_.shaders.geometry.value().shader_module.cast_to<const ShaderModuleD3D12>();
        pipeline_desc.GS = shader_dx->raw_bytecode();
    }
    {
        auto shader_dx = desc_.shaders.fragment.shader_module.cast_to<const ShaderModuleD3D12>();
        pipeline_desc.PS = shader_dx->raw_bytecode();
    }

    pipeline_desc.pRootSignature = root_signature_.Get();

    pipeline_desc.NumRenderTargets = desc_.color_target_state.attachments.size();
    for (size_t i = 0; i < desc_.color_target_state.attachments.size(); i++) {
        auto const& attachment = desc_.color_target_state.attachments[i];
        pipeline_desc.RTVFormats[i] = to_dx_format(attachment.format);
        pipeline_desc.BlendState.RenderTarget[i] = D3D12_RENDER_TARGET_BLEND_DESC{
            .BlendEnable = attachment.blend_enable,
            .LogicOpEnable = false,
            .SrcBlend = to_dx_blend_factor(attachment.src_blend_factor),
            .DestBlend = to_dx_blend_factor(attachment.dst_blend_factor),
            .BlendOp = to_dx_blend_op(attachment.blend_op),
            .SrcBlendAlpha = to_dx_blend_factor(attachment.src_alpha_blend_factor),
            .DestBlendAlpha = to_dx_blend_factor(attachment.dst_alpha_blend_factor),
            .BlendOpAlpha = to_dx_blend_op(attachment.alpha_blend_op),
            .LogicOp = D3D12_LOGIC_OP_NOOP,
            .RenderTargetWriteMask = attachment.color_write_mask.raw_value(),
        };
    }

    pipeline_desc.SampleMask = ~0u;

    pipeline_desc.RasterizerState = D3D12_RASTERIZER_DESC{
        .FillMode = to_dx_fill_mode(desc_.rasterization_state.polygon_mode),
        .CullMode = to_dx_cull_mode(desc_.rasterization_state.cull_mode),
        .FrontCounterClockwise = desc_.rasterization_state.front_face == FrontFace::ccw,
        .DepthBias = 0,
        .DepthBiasClamp = 0.0f,
        .SlopeScaledDepthBias = 0.0f,
        .DepthClipEnable = false,
        .MultisampleEnable = false,
        .AntialiasedLineEnable = false,
        .ForcedSampleCount = 0,
        .ConservativeRaster = desc_.rasterization_state.conservative
            ? D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON : D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
    };

    pipeline_desc.DepthStencilState = D3D12_DEPTH_STENCIL_DESC{
        .DepthEnable = desc_.depth_stencil_state.format == ResourceFormat::undefined
            ? false : desc_.depth_stencil_state.depth_test,
        .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
        .DepthFunc = to_dx_compare_op(desc_.depth_stencil_state.depth_compare_op),
        .StencilEnable = desc_.depth_stencil_state.stencil_test,
        .StencilReadMask = desc_.depth_stencil_state.stencil_compare_mask,
        .StencilWriteMask = desc_.depth_stencil_state.stencil_write_mask,
        .FrontFace = D3D12_DEPTH_STENCILOP_DESC{
            .StencilFailOp = to_dx_stencil_op(desc_.depth_stencil_state.stencil_front_face.fail_op),
            .StencilDepthFailOp = to_dx_stencil_op(desc_.depth_stencil_state.stencil_front_face.depth_fail_op),
            .StencilPassOp = to_dx_stencil_op(desc_.depth_stencil_state.stencil_front_face.pass_op),
            .StencilFunc = to_dx_compare_op(desc_.depth_stencil_state.stencil_front_face.compare_op),
        },
        .BackFace = D3D12_DEPTH_STENCILOP_DESC{
            .StencilFailOp = to_dx_stencil_op(desc_.depth_stencil_state.stencil_back_face.fail_op),
            .StencilDepthFailOp = to_dx_stencil_op(desc_.depth_stencil_state.stencil_back_face.depth_fail_op),
            .StencilPassOp = to_dx_stencil_op(desc_.depth_stencil_state.stencil_back_face.pass_op),
            .StencilFunc = to_dx_compare_op(desc_.depth_stencil_state.stencil_back_face.compare_op),
        },
    };
    pipeline_desc.DSVFormat = to_dx_format(desc_.depth_stencil_state.format);

    std::vector<D3D12_INPUT_ELEMENT_DESC> input_elements{};
    for (size_t i = 0; i < desc_.vertex_input_buffers.size(); i++) {
        auto const& input_buffer = desc_.vertex_input_buffers[i];
        for (auto const& attribute : input_buffer.attributes) {
            uint32_t location = static_cast<uint32_t>(attribute.semantics);
            input_elements.push_back(D3D12_INPUT_ELEMENT_DESC{
                .SemanticName = to_dx_semantic_name(attribute.semantics),
                .SemanticIndex = to_dx_semantic_index(attribute.semantics),
                .Format = to_dx_format(attribute.format),
                .InputSlot = static_cast<UINT>(i),
                .AlignedByteOffset = attribute.offset,
                .InputSlotClass = input_buffer.per_instance
                    ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = input_buffer.per_instance ? 1u : 0u,
            });
        }
    }
    pipeline_desc.InputLayout = D3D12_INPUT_LAYOUT_DESC{
        .pInputElementDescs = input_elements.data(),
        .NumElements = static_cast<UINT>(input_elements.size()),
    };

    pipeline_desc.PrimitiveTopologyType = to_dx_primitive_topology_type(desc_.rasterization_state.topology);
    primitive_topology_ = to_dx_primitive_topology(desc_.rasterization_state.topology);

    pipeline_desc.SampleDesc = DXGI_SAMPLE_DESC{.Count = 1, .Quality = 0};

    pipeline_desc.NodeMask = 0;

    pipeline_desc.CachedPSO = {};

    pipeline_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    if (device_->pipeline_library()) {
        auto pipeline_str = hash_graphics_pipeline_desc(pipeline_desc);
        auto pipeline_wstr = chars_to_wstring(pipeline_str.c_str(), pipeline_str.size());
        auto load_ret = device_->pipeline_library()->LoadGraphicsPipeline(
            pipeline_wstr.c_str(), &pipeline_desc, IID_PPV_ARGS(&pipeline_)
        );
        if (!SUCCEEDED(load_ret)) {
            device_->raw()->CreateGraphicsPipelineState(&pipeline_desc, IID_PPV_ARGS(&pipeline_));
            device_->pipeline_library()->StorePipeline(pipeline_wstr.c_str(), pipeline_.Get());
        }
    } else {
        device_->raw()->CreateGraphicsPipelineState(&pipeline_desc, IID_PPV_ARGS(&pipeline_));
    }
}


ComputePipelineD3D12::ComputePipelineD3D12(Ref<DeviceD3D12> device, ComputePipelineDesc const& desc)
    : ComputePipeline(desc), device_(device)
{
    create_pipeline_layout(
        device_, root_signature_,
        desc_.bind_groups_layout, desc_.static_samplers, desc_.push_constants, false
    );
    root_constant_index_ = desc_.bind_groups_layout.size();

    auto shader_dx = desc.compute.shader_module.cast_to<const ShaderModuleD3D12>();

    D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline_desc{
        .pRootSignature = root_signature_.Get(),
        .CS = shader_dx->raw_bytecode(),
        .NodeMask = 0,
        .CachedPSO = {},
        .Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
    };

    if (device_->pipeline_library()) {
        auto pipeline_str = hash_compute_pipeline_desc(pipeline_desc);
        auto pipeline_wstr = chars_to_wstring(pipeline_str.c_str(), pipeline_str.size());
        auto load_ret = device_->pipeline_library()->LoadComputePipeline(
            pipeline_wstr.c_str(), &pipeline_desc, IID_PPV_ARGS(&pipeline_)
        );
        if (!SUCCEEDED(load_ret)) {
            device->raw()->CreateComputePipelineState(&pipeline_desc, IID_PPV_ARGS(&pipeline_));
            device_->pipeline_library()->StorePipeline(pipeline_wstr.c_str(), pipeline_.Get());
        }
    } else {
        device->raw()->CreateComputePipelineState(&pipeline_desc, IID_PPV_ARGS(&pipeline_));
    }
}

}
