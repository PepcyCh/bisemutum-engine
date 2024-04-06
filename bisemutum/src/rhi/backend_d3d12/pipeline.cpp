#include "pipeline.hpp"

#include <algorithm>

#include <bisemutum/prelude/misc.hpp>
#include <bisemutum/prelude/math.hpp>
#include <bisemutum/runtime/logger.hpp>

#include "device.hpp"
#include "shader.hpp"
#include "sampler.hpp"
#include "utils.hpp"

namespace bi::rhi {

namespace {

auto chars_to_wstring(std::string_view str) -> std::wstring {
    auto length = MultiByteToWideChar(CP_UTF8, 0, str.data(), str.size(), nullptr, 0);
    std::wstring ret(length, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.data(), str.size(), ret.data(), length);
    return ret;
}

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
        case BlendOp::reverse_subtract: return D3D12_BLEND_OP_REV_SUBTRACT;
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

    device_->pipeline_cache().create_graphics_pipeline(desc_, pipeline_desc, pipeline_);
}


ComputePipelineD3D12::ComputePipelineD3D12(Ref<DeviceD3D12> device, ComputePipelineDesc const& desc)
    : ComputePipeline(desc), device_(device)
{
    create_pipeline_layout(
        device_, root_signature_,
        desc_.bind_groups_layout, desc_.static_samplers, desc_.push_constants, false
    );
    root_constant_index_ = desc_.bind_groups_layout.size();

    auto shader_dx = desc_.compute.shader_module.cast_to<const ShaderModuleD3D12>();

    D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline_desc{
        .pRootSignature = root_signature_.Get(),
        .CS = shader_dx->raw_bytecode(),
        .NodeMask = 0,
        .CachedPSO = {},
        .Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
    };

    device_->pipeline_cache().create_compute_pipeline(desc_, pipeline_desc, pipeline_);
}


RaytracingPipelineD3D12::RaytracingPipelineD3D12(Ref<DeviceD3D12> device, RaytracingPipelineDesc const& desc)
    : RaytracingPipeline(desc), device_(device)
{
    create_pipeline_layout(
        device_, root_signature_,
        desc_.bind_groups_layout, desc_.static_samplers, desc_.push_constants, false
    );
    root_constant_index_ = desc_.bind_groups_layout.size();

    std::vector<D3D12_STATE_SUBOBJECT> subobjects;

    std::list<D3D12_DXIL_LIBRARY_DESC> shaders;
    std::list<D3D12_EXPORT_DESC> shader_exports;
    std::list<D3D12_HIT_GROUP_DESC> hit_groups;
    std::list<D3D12_LOCAL_ROOT_SIGNATURE> local_root_signatures;
    std::list<D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION> lrs_associations;
    std::list<std::wstring> owned_strings;

    const auto add_subobject = [&](D3D12_STATE_SUBOBJECT_TYPE type, void* ptr) {
        auto& subobject = subobjects.emplace_back();
        subobject.Type = type;
        subobject.pDesc = ptr;
    };
    const auto add_dxil_shader = [&](PipelineShader const& pipeline_shader, std::wstring* p_wstr = nullptr) {
        auto& shader = shaders.emplace_back();
        auto& export_desc = shader_exports.emplace_back();

        shader.DXILLibrary = pipeline_shader.shader_module.cast_to<const ShaderModuleD3D12>()->raw_bytecode();
        shader.NumExports = 1;
        shader.pExports = &export_desc;

        static size_t num_shader = 0;

        auto& original_name = owned_strings.emplace_back();
        original_name = chars_to_wstring(pipeline_shader.entry);
        if (!p_wstr) {
            p_wstr = &owned_strings.emplace_back();
        }
        *p_wstr = L"dxil_shader_" + std::to_wstring(num_shader++);;

        export_desc.Name = p_wstr->c_str();
        export_desc.Flags = D3D12_EXPORT_FLAG_NONE;
        export_desc.ExportToRename = original_name.c_str();

        add_subobject(D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &shader);
    };

    // DXIL shaders
    add_dxil_shader(desc_.shaders.raygen.shader, &export_names_.raygen);
    export_names_.miss.resize(desc_.shaders.miss.size());
    for (size_t i = 0; auto& miss : desc_.shaders.miss) {
        add_dxil_shader(miss.shader, &export_names_.miss[i]);
        ++i;
    }
    export_names_.hit_group.resize(desc_.shaders.hit_group.size());
    for (size_t i = 0; auto& hit : desc_.shaders.hit_group) {
        auto& hit_group = hit_groups.emplace_back();
        hit_group.Type = hit.intersection.has_value()
            ? D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE
            : D3D12_HIT_GROUP_TYPE_TRIANGLES;
        hit_group.ClosestHitShaderImport = nullptr;
        hit_group.AnyHitShaderImport = nullptr;
        hit_group.IntersectionShaderImport = nullptr;

        export_names_.hit_group[i] = L"hit_group_" + std::to_wstring(i);
        hit_group.HitGroupExport = export_names_.hit_group[i].c_str();

        if (auto& shader = hit.closest_hit; shader) {
            add_dxil_shader(shader.value());
            hit_group.ClosestHitShaderImport = shader_exports.back().Name;
        }
        if (auto& shader = hit.any_hit; shader) {
            add_dxil_shader(shader.value());
            hit_group.AnyHitShaderImport = shader_exports.back().Name;
        }
        if (auto& shader = hit.intersection; shader) {
            add_dxil_shader(shader.value());
            hit_group.IntersectionShaderImport = shader_exports.back().Name;
        }

        add_subobject(D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &hit_group);

        ++i;
    }
    export_names_.callable.resize(desc_.shaders.callable.size());
    for (size_t i = 0; auto& callable : desc_.shaders.callable) {
        add_dxil_shader(callable.shader, &export_names_.callable[i]);
        ++i;
    }

    // shader & pipeline config
    D3D12_RAYTRACING_SHADER_CONFIG shader_config{
        .MaxPayloadSizeInBytes = desc_.state.max_ray_payload_size,
        .MaxAttributeSizeInBytes = desc_.state.max_hit_attribute_size,
    };
    add_subobject(D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &shader_config);
    D3D12_RAYTRACING_PIPELINE_CONFIG1 pipeline_config{
        .MaxTraceRecursionDepth = desc_.state.max_recursive_depth,
        .Flags = static_cast<D3D12_RAYTRACING_PIPELINE_FLAGS>(
            desc_.state.skip_procedural ? D3D12_RAYTRACING_PIPELINE_FLAG_SKIP_PROCEDURAL_PRIMITIVES : 0
        ),
    };
    add_subobject(D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG1, &pipeline_config);

    // global root signature
    D3D12_GLOBAL_ROOT_SIGNATURE global_root_signature{
        .pGlobalRootSignature = root_signature_.Get(),
    };
    add_subobject(D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &global_root_signature);

    // local root signature & associations (must be at the last)
    std::unordered_map<ID3D12RootSignature*, std::vector<wchar_t const*>> lrs_associations_map;
    size_t num_associations = 0;
    if (desc_.shader_record_sizes.raygen > 0) {
        auto lrs = device_->get_local_root_signature(
            desc_.shader_record_sizes.raygen,
            d3d12_local_root_signature_space, d3d12_local_root_signature_register_raygen
        );
        lrs_associations_map[lrs].push_back(export_names_.raygen.c_str());
        ++num_associations;
    }
    if (desc_.shader_record_sizes.miss > 0) {
        auto lrs = device_->get_local_root_signature(
            desc_.shader_record_sizes.miss,
            d3d12_local_root_signature_space, d3d12_local_root_signature_register_miss
        );
        for (auto& name : export_names_.miss) {
            lrs_associations_map[lrs].push_back(name.c_str());
        }
        num_associations += export_names_.miss.size();
    }
    if (desc_.shader_record_sizes.hit_group > 0) {
        auto lrs = device_->get_local_root_signature(
            desc_.shader_record_sizes.hit_group,
            d3d12_local_root_signature_space, d3d12_local_root_signature_register_hit_group
        );
        for (auto& name : export_names_.hit_group) {
            lrs_associations_map[lrs].push_back(name.c_str());
        }
        num_associations += export_names_.hit_group.size();
    }
    if (desc_.shader_record_sizes.callable > 0) {
        auto lrs = device_->get_local_root_signature(
            desc_.shader_record_sizes.callable,
            d3d12_local_root_signature_space, d3d12_local_root_signature_register_callable
        );
        for (auto& name : export_names_.callable) {
            lrs_associations_map[lrs].push_back(name.c_str());
        }
        num_associations += export_names_.callable.size();
    }
    // Call `reserve` to make sure that taking pointer to `subobjects` is safe.
    subobjects.reserve(subobjects.size() + lrs_associations_map.size() + num_associations);
    for (auto& [rs, association_vec] : lrs_associations_map) {
        auto& lrs = local_root_signatures.emplace_back();
        lrs.pLocalRootSignature = rs;
        add_subobject(D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE, &lrs);
        auto& association = lrs_associations.emplace_back();
        association.pSubobjectToAssociate = &subobjects.back();
        association.NumExports = association_vec.size();
        association.pExports = association_vec.data();
        add_subobject(D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, &association);
    }

    D3D12_STATE_OBJECT_DESC pipeline_state{
        .Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE,
        .NumSubobjects = static_cast<uint32_t>(subobjects.size()),
        .pSubobjects = subobjects.data(),
    };

    device_->raw()->CreateStateObject(&pipeline_state, IID_PPV_ARGS(&pipeline_));
}

auto RaytracingPipelineD3D12::get_shader_binding_table_sizes() const -> RaytracingShaderBindingTableSizes {
    return RaytracingShaderBindingTableSizes{
        .raygen_size = aligned_size<uint32_t>(
            D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + desc_.shader_record_sizes.raygen,
            D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT
        ),
        .miss_stride = aligned_size<uint32_t>(
            D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + desc_.shader_record_sizes.miss,
            D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT
        ),
        .hit_group_stride = aligned_size<uint32_t>(
            D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + desc_.shader_record_sizes.hit_group,
            D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT
        ),
        .callable_stride = aligned_size<uint32_t>(
            D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + desc_.shader_record_sizes.callable,
            D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT
        ),
    };
}

auto RaytracingPipelineD3D12::get_shader_handle(
    RaytracingShaderBindingTableType type, uint32_t from_index, uint32_t count, void* dst_data
) const -> void {
    Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> state_props;
    pipeline_.As(&state_props);

    switch (type) {
        case RaytracingShaderBindingTableType::raygen:
            std::memcpy(
                dst_data, state_props->GetShaderIdentifier(export_names_.raygen.c_str()),
                D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES
            );
            break;
        case RaytracingShaderBindingTableType::miss: {
            auto to_index = std::min<uint32_t>(from_index + count, export_names_.miss.size());
            for (auto i = from_index; i < to_index; i++) {
                std::memcpy(
                    static_cast<std::byte*>(dst_data) + (i - from_index) * D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES,
                    state_props->GetShaderIdentifier(export_names_.miss[i].c_str()),
                    D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES
                );
            }
            break;
        }
        case RaytracingShaderBindingTableType::hit_group: {
            auto to_index = std::min<uint32_t>(from_index + count, export_names_.hit_group.size());
            for (auto i = from_index; i < to_index; i++) {
                std::memcpy(
                    static_cast<std::byte*>(dst_data) + (i - from_index) * D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES,
                    state_props->GetShaderIdentifier(export_names_.hit_group[i].c_str()),
                    D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES
                );
            }
            break;
        }
        case RaytracingShaderBindingTableType::callable: {
            auto to_index = std::min<uint32_t>(from_index + count, export_names_.callable.size());
            for (auto i = from_index; i < to_index; i++) {
                std::memcpy(
                    static_cast<std::byte*>(dst_data) + (i - from_index) * D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES,
                    state_props->GetShaderIdentifier(export_names_.callable[i].c_str()),
                    D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES
                );
            }
            break;
        }
    }
}

}
