#include "pipeline.hpp"

#include "device.hpp"
#include "utils.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

namespace {

void CreateLayout(const ShaderInfoVulkan &shader_info, VkDevice device, Vec<VkDescriptorSetLayout> &set_layouts,
    VkPipelineLayout &pipeline_layout) {
    set_layouts.resize(shader_info.bindings.bindings.size());
    for (size_t set = 0; set < set_layouts.size(); set++) {
        const auto &binding_info = shader_info.bindings.bindings[set];
        VkDescriptorSetLayoutCreateInfo set_layout_ci {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .bindingCount = static_cast<uint32_t>(binding_info.size()),
            .pBindings = binding_info.data(),
        };
        vkCreateDescriptorSetLayout(device, &set_layout_ci, nullptr, &set_layouts[set]);
    }

    VkPushConstantRange push_constant {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = shader_info.bindings.push_constant_size,
    };
    VkPipelineLayoutCreateInfo pipeline_layout_ci {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = static_cast<uint32_t>(set_layouts.size()),
        .pSetLayouts = set_layouts.data(),
        .pushConstantRangeCount = shader_info.bindings.push_constant_size > 0 ? 1u : 0u,
        .pPushConstantRanges = shader_info.bindings.push_constant_size > 0 ? &push_constant : nullptr,
    };
    vkCreatePipelineLayout(device, &pipeline_layout_ci, nullptr, &pipeline_layout);
}

VkPrimitiveTopology ToVkPrimitiveTopology(PrimitiveTopology topo) {
    switch (topo) {
        case PrimitiveTopology::ePointList: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case PrimitiveTopology::eLineList: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case PrimitiveTopology::eLineStrip: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case PrimitiveTopology::eTriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case PrimitiveTopology::eTriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case PrimitiveTopology::eTriangleFan: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
        case PrimitiveTopology::eLineListAdjacency: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
        case PrimitiveTopology::eLineStripAdjacency: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
        case PrimitiveTopology::eTriangleListAdjacency: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
        case PrimitiveTopology::eTriangleStripAdjacency: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
        case PrimitiveTopology::ePatchList: return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    }
    Unreachable();
}

VkPolygonMode ToVkPolygonMode(PolygonMode poly) {
    switch (poly) {
        case PolygonMode::eFill: return VK_POLYGON_MODE_FILL;
        case PolygonMode::eLine: return VK_POLYGON_MODE_LINE;
        case PolygonMode::ePoint: return VK_POLYGON_MODE_POINT;
    }
    Unreachable();
}

VkCullModeFlags ToVkCullMode(CullMode cull) {
    switch (cull) {
        case CullMode::eNone: return VK_CULL_MODE_NONE;
        case CullMode::eBackFace: return VK_CULL_MODE_BACK_BIT;
        case CullMode::eFrontFace: return VK_CULL_MODE_FRONT_BIT;
    }
    Unreachable();
}

VkFrontFace ToVkFrontFace(FrontFace face) {
    return face == FrontFace::eCcw ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
}

VkStencilOp ToVkStencilOp(StencilOp op) {
    return static_cast<VkStencilOp>(op);
}

VkBlendOp ToVkBlendOp(BlendOp op) {
    switch (op) {
        case BlendOp::eAdd: return VK_BLEND_OP_ADD;
        case BlendOp::eSubtract: return VK_BLEND_OP_SUBTRACT;
        case BlendOp::eReserveSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
        case BlendOp::eMin: return VK_BLEND_OP_MIN;
        case BlendOp::eMax: return VK_BLEND_OP_MAX;
    }
    Unreachable();
}

VkBlendFactor ToVkBlendFactor(BlendFactor factor) {
    switch (factor) {
        case BlendFactor::eZero: return VK_BLEND_FACTOR_ZERO;
        case BlendFactor::eOne: return VK_BLEND_FACTOR_ONE;
        case BlendFactor::eSrc: return VK_BLEND_FACTOR_SRC_COLOR;
        case BlendFactor::eOneMinusSrc: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case BlendFactor::eSrcAlpha: return VK_BLEND_FACTOR_SRC_ALPHA;
        case BlendFactor::eOneMinusSrcAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::eDst: return VK_BLEND_FACTOR_DST_COLOR;
        case BlendFactor::eOneMinusDst: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case BlendFactor::eDstAlpha: return VK_BLEND_FACTOR_DST_ALPHA;
        case BlendFactor::eOneMinusDstAlpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case BlendFactor::eSrcAlphaSaturated: return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        case BlendFactor::eConstant: return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case BlendFactor::eOneMinusConstant: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    }
    Unreachable();
}

VkColorComponentFlags ToVkColorWriteMask(BitFlags<ColorWriteComponent> mask) {
    return static_cast<VkColorComponentFlags>(mask.RawValue());
}

}

RenderPipelineVulkan::RenderPipelineVulkan(Ref<DeviceVulkan> device, const RenderPipelineDesc &desc)
    : device_(device), desc_(desc) {

    ShaderInfoVulkan shader_info {};
    Vec<VkPipelineShaderStageCreateInfo> stages {};
    {
        auto shader_vk = desc.shaders.vertex.CastTo<ShaderModuleVulkan>();
        shader_info = shader_vk->Info();
        stages.push_back(shader_vk->RawPipelineShaderStage());
    }
    if (desc.shaders.tessellation_control) {
        auto shader_vk = static_cast<ShaderModuleVulkan *>(desc.shaders.tessellation_control);
        shader_info.bindings.Combine(shader_vk->Info().bindings);
        stages.push_back(shader_vk->RawPipelineShaderStage());
    }
    if (desc.shaders.tessellation_evaluation) {
        auto shader_vk = static_cast<ShaderModuleVulkan *>(desc.shaders.tessellation_evaluation);
        shader_info.bindings.Combine(shader_vk->Info().bindings);
        stages.push_back(shader_vk->RawPipelineShaderStage());
    }
    if (desc.shaders.geometry) {
        auto shader_vk = static_cast<ShaderModuleVulkan *>(desc.shaders.geometry);
        shader_info.bindings.Combine(shader_vk->Info().bindings);
        stages.push_back(shader_vk->RawPipelineShaderStage());
    }
    {
        auto shader_vk = desc.shaders.fragment.CastTo<ShaderModuleVulkan>();
        shader_info.bindings.Combine(shader_vk->Info().bindings);
        stages.push_back(shader_vk->RawPipelineShaderStage());
    }

    CreateLayout(shader_info, device->Raw(), set_layouts_, pipeline_layout_);

    Vec<VkVertexInputAttributeDescription> vertex_input_attribute_descs;
    Vec<size_t> vertex_input_attributes_map(shader_info.vertex_inputs.size(), -1);
    vertex_input_attribute_descs.reserve(shader_info.vertex_inputs.size());
    for (size_t i = 0; i < shader_info.vertex_inputs.size(); i++) {
        const auto &input_attribute = shader_info.vertex_inputs[i];
        if (input_attribute == ResourceFormat::eUndefined) {
            continue;
        }
        vertex_input_attributes_map[i] = vertex_input_attribute_descs.size();
        vertex_input_attribute_descs.push_back(VkVertexInputAttributeDescription {
            .location = static_cast<uint32_t>(i),
            .format = ToVkFormat(input_attribute),
        });
    }
    Vec<VkVertexInputBindingDescription> vertex_input_binding_descs(desc.vertex_input_buffers.size());
    for (size_t i = 0; i < desc.vertex_input_buffers.size(); i++) {
        const auto &input_buffer = desc.vertex_input_buffers[i];
        vertex_input_binding_descs[i] = VkVertexInputBindingDescription {
            .binding = static_cast<uint32_t>(i),
            .stride = input_buffer.stride,
            .inputRate = input_buffer.per_instance ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX,
        };
        for (const auto &attribute : input_buffer.attributes) {
            uint32_t location = static_cast<uint32_t>(attribute.semantics);
            if (vertex_input_attributes_map[location] != -1) {
                size_t index = vertex_input_attributes_map[location];
                vertex_input_attribute_descs[index].binding = static_cast<uint32_t>(i);
                vertex_input_attribute_descs[index].offset = attribute.offset;
            }
        }
    }
    VkPipelineVertexInputStateCreateInfo vertex_input_state {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .vertexBindingDescriptionCount = static_cast<uint32_t>(vertex_input_binding_descs.size()),
        .pVertexBindingDescriptions = vertex_input_binding_descs.data(),
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertex_input_attribute_descs.size()),
        .pVertexAttributeDescriptions = vertex_input_attribute_descs.data(),
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .topology = ToVkPrimitiveTopology(desc.primitive_state.topology),
        .primitiveRestartEnable = VK_FALSE,
    };

    VkPipelineViewportStateCreateInfo viewport_state {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = nullptr,
        .scissorCount = 1,
        .pScissors = nullptr,
    };

    VkPipelineRasterizationStateCreateInfo rasterization_state {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = ToVkPolygonMode(desc.primitive_state.polygon_mode),
        .cullMode = ToVkCullMode(desc.primitive_state.cull_mode),
        .frontFace = ToVkFrontFace(desc.primitive_state.front_face),
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f,
        .lineWidth = 0.0f,
    };
    VkPipelineRasterizationConservativeStateCreateInfoEXT conservative_rasterization {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT,
        .pNext = nullptr,
        .flags = 0,
        .conservativeRasterizationMode = desc.primitive_state.conservative
            ? VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT
            : VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT,
        .extraPrimitiveOverestimationSize = 0.0f,
    };
    ConnectVkPNextChain(&rasterization_state, &conservative_rasterization);

    VkPipelineMultisampleStateCreateInfo multosample_state {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 0.0f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil_state {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthTestEnable = desc.depth_stencil_state.depth_test,
        .depthWriteEnable = desc.depth_stencil_state.depth_write,
        .depthCompareOp = ToVkCompareOp(desc.depth_stencil_state.depth_compare_op),
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = desc.depth_stencil_state.stencil_test,
        .front = VkStencilOpState {
            .failOp = ToVkStencilOp(desc.depth_stencil_state.stencil_front_face.fail_op),
            .passOp = ToVkStencilOp(desc.depth_stencil_state.stencil_front_face.pass_op),
            .depthFailOp = ToVkStencilOp(desc.depth_stencil_state.stencil_front_face.depth_fail_op),
            .compareOp = ToVkCompareOp(desc.depth_stencil_state.stencil_front_face.compare_op),
            .compareMask = desc.depth_stencil_state.stencil_compare_mask,
            .writeMask = desc.depth_stencil_state.stencil_write_mask,
            .reference = desc.depth_stencil_state.stencil_reference,
        },
        .back = VkStencilOpState {
            .failOp = ToVkStencilOp(desc.depth_stencil_state.stencil_back_face.fail_op),
            .passOp = ToVkStencilOp(desc.depth_stencil_state.stencil_back_face.pass_op),
            .depthFailOp = ToVkStencilOp(desc.depth_stencil_state.stencil_back_face.depth_fail_op),
            .compareOp = ToVkCompareOp(desc.depth_stencil_state.stencil_back_face.compare_op),
            .compareMask = desc.depth_stencil_state.stencil_compare_mask,
            .writeMask = desc.depth_stencil_state.stencil_write_mask,
            .reference = desc.depth_stencil_state.stencil_reference,
        },
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
    };

    Vec<VkPipelineColorBlendAttachmentState> color_blend_attachments(desc.color_blend_state.attachments.size());
    for (size_t i = 0; i < color_blend_attachments.size(); i++) {
        const auto attachment = desc.color_blend_state.attachments[i];
        color_blend_attachments[i] = VkPipelineColorBlendAttachmentState {
            .blendEnable = attachment.blend_enable,
            .srcColorBlendFactor = ToVkBlendFactor(attachment.src_blend_factor),
            .dstColorBlendFactor = ToVkBlendFactor(attachment.dst_blend_factor),
            .colorBlendOp = ToVkBlendOp(attachment.blend_op),
            .srcAlphaBlendFactor = ToVkBlendFactor(attachment.src_alpha_blend_factor),
            .dstAlphaBlendFactor = ToVkBlendFactor(attachment.dst_alpha_blend_factor),
            .alphaBlendOp = ToVkBlendOp(attachment.alpha_blend_op),
            .colorWriteMask = ToVkColorWriteMask(attachment.color_write_mask),
        };
    }
    VkPipelineColorBlendStateCreateInfo color_blend_state {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_NO_OP,
        .attachmentCount = static_cast<uint32_t>(color_blend_attachments.size()),
        .pAttachments = color_blend_attachments.data(),
        .blendConstants = {
            desc.color_blend_state.blend_constants.r,
            desc.color_blend_state.blend_constants.g,
            desc.color_blend_state.blend_constants.b,
            desc.color_blend_state.blend_constants.a,
        }
    };

    Vec<VkDynamicState> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamic_state {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data(),
    };

    VkGraphicsPipelineCreateInfo pipeline_ci {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stageCount = static_cast<uint32_t>(stages.size()),
        .pStages = stages.data(),
        .pVertexInputState = &vertex_input_state,
        .pInputAssemblyState = &input_assembly_state,
        .pTessellationState = nullptr,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterization_state,
        .pMultisampleState = &multosample_state,
        .pDepthStencilState = &depth_stencil_state,
        .pColorBlendState = &color_blend_state,
        .pDynamicState = &dynamic_state,
        .layout = pipeline_layout_,
        .renderPass = VK_NULL_HANDLE,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    };
    vkCreateGraphicsPipelines(device->Raw(), VK_NULL_HANDLE, 1, &pipeline_ci, nullptr, &pipeline_);
}

RenderPipelineVulkan::~RenderPipelineVulkan() {
    for (auto &set_layout : set_layouts_) {
        vkDestroyDescriptorSetLayout(device_->Raw(), set_layout, nullptr);
    }
    vkDestroyPipelineLayout(device_->Raw(), pipeline_layout_, nullptr);
    vkDestroyPipeline(device_->Raw(), pipeline_, nullptr);
}

ComputePipelineVulkan::ComputePipelineVulkan(Ref<DeviceVulkan> device, const ComputePipelineDesc &desc)
    : device_(device), desc_(desc) {
    auto shader_vk = desc.compute.CastTo<ShaderModuleVulkan>();
    const auto &shader_info = shader_vk->Info();

    local_size_x = shader_info.compute_local_size_x;
    local_size_y = shader_info.compute_local_size_y;
    local_size_z = shader_info.compute_local_size_z;

    CreateLayout(shader_info, device->Raw(), set_layouts_, pipeline_layout_);

    VkComputePipelineCreateInfo pipeline_ci {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = shader_vk->RawPipelineShaderStage(),
        .layout = pipeline_layout_,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    };
    vkCreateComputePipelines(device->Raw(), VK_NULL_HANDLE, 1, &pipeline_ci, nullptr, &pipeline_);
}

ComputePipelineVulkan::~ComputePipelineVulkan() {
    for (auto &set_layout : set_layouts_) {
        vkDestroyDescriptorSetLayout(device_->Raw(), set_layout, nullptr);
    }
    vkDestroyPipelineLayout(device_->Raw(), pipeline_layout_, nullptr);
    vkDestroyPipeline(device_->Raw(), pipeline_, nullptr);
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
