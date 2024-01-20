#include "pipeline.hpp"

#include <bisemutum/prelude/misc.hpp>
#include <bisemutum/prelude/math.hpp>

#include "volk.h"
#include "device.hpp"
#include "shader.hpp"
#include "sampler.hpp"
#include "descriptor.hpp"
#include "utils.hpp"

namespace bi::rhi {

namespace {

auto to_vk_pipeline_shader_stage(
    PipelineShader const& shader, VkShaderStageFlagBits stage, char const* entry
) -> VkPipelineShaderStageCreateInfo {
    return VkPipelineShaderStageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = stage,
        .module = shader.shader_module.cast_to<ShaderModuleVulkan const>()->raw(),
        .pName = entry,
        .pSpecializationInfo = nullptr,
    };
}

auto create_pipeline_layout(
    Ref<DeviceVulkan> device,
    std::vector<VkDescriptorSetLayout>& set_layouts,
    VkPipelineLayout& pipeline_layout,
    std::vector<BindGroupLayout> const& bind_groups_layout,
    std::vector<StaticSampler> const& static_samplers,
    PushConstantsDesc const& push_constants_desc
) -> void {
    set_layouts.resize(bind_groups_layout.size() + !static_samplers.empty());
    for (size_t set = 0; auto const& group : bind_groups_layout) {
        set_layouts[set] = device->require_descriptor_set_layout(group);
        ++set;
    }
    std::vector<VkSampler> embedded_samplers(static_samplers.size());
    if (!static_samplers.empty()) {
        std::vector<VkDescriptorSetLayoutBinding> bindings_info(static_samplers.size());
        for (size_t i = 0; auto const &entry : static_samplers) {
            auto sampler = entry.sampler.cast_to<const SamplerVulkan>();
            embedded_samplers[i] = sampler->raw();
            bindings_info[i].binding = entry.binding_or_register;
            bindings_info[i].descriptorCount = 1;
            bindings_info[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            bindings_info[i].pImmutableSamplers = &embedded_samplers[i];
            bindings_info[i].stageFlags = to_vk_shader_stages(entry.visibility);
            ++i;
        }
        VkDescriptorSetLayoutCreateFlags flags = device->use_descriptor_buffer()
            ? (VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
                | VK_DESCRIPTOR_SET_LAYOUT_CREATE_EMBEDDED_IMMUTABLE_SAMPLERS_BIT_EXT)
            : 0u;
        VkDescriptorSetLayoutCreateInfo set_layout_ci{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = flags,
            .bindingCount = static_cast<uint32_t>(bindings_info.size()),
            .pBindings = bindings_info.data(),
        };
        vkCreateDescriptorSetLayout(device->raw(), &set_layout_ci, nullptr, &set_layouts.back());
    }

    auto has_push_constants = push_constants_desc.size > 0;
    VkPushConstantRange push_constant{
        .stageFlags = to_vk_shader_stages(push_constants_desc.visibility),
        .offset = 0,
        .size = push_constants_desc.size,
    };
    VkPipelineLayoutCreateInfo pipeline_layout_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = static_cast<uint32_t>(set_layouts.size()),
        .pSetLayouts = set_layouts.data(),
        .pushConstantRangeCount = has_push_constants ? 1u : 0u,
        .pPushConstantRanges = has_push_constants ? &push_constant : nullptr,
    };
    vkCreatePipelineLayout(device->raw(), &pipeline_layout_ci, nullptr, &pipeline_layout);
}

auto to_vk_primitive_topology(PrimitiveTopology topo) -> VkPrimitiveTopology {
    switch (topo) {
        case PrimitiveTopology::point_list: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case PrimitiveTopology::line_list: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case PrimitiveTopology::line_strip: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case PrimitiveTopology::triangle_list: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case PrimitiveTopology::triangle_strip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case PrimitiveTopology::line_list_adjacency: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
        case PrimitiveTopology::line_strip_adjacency: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
        case PrimitiveTopology::triangle_list_adjacency: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
        case PrimitiveTopology::triangle_strip_adjacency: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
        case PrimitiveTopology::patch_list: return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    }
    unreachable();
}

auto is_primitive_list(PrimitiveTopology topo) -> bool {
    return topo == PrimitiveTopology::point_list
        || topo == PrimitiveTopology::line_list
        || topo == PrimitiveTopology::triangle_list
        || topo == PrimitiveTopology::line_list_adjacency
        || topo == PrimitiveTopology::triangle_list_adjacency
        || topo == PrimitiveTopology::patch_list;
}

auto to_vk_polygon_mode(PolygonMode poly) -> VkPolygonMode {
    switch (poly) {
        case PolygonMode::fill: return VK_POLYGON_MODE_FILL;
        case PolygonMode::line: return VK_POLYGON_MODE_LINE;
        case PolygonMode::point: return VK_POLYGON_MODE_POINT;
    }
    unreachable();
}

auto to_vk_cull_mode(CullMode cull) -> VkCullModeFlags {
    switch (cull) {
        case CullMode::none: return VK_CULL_MODE_NONE;
        case CullMode::back_face: return VK_CULL_MODE_BACK_BIT;
        case CullMode::front_face: return VK_CULL_MODE_FRONT_BIT;
    }
    unreachable();
}

auto to_vk_front_face(FrontFace face) -> VkFrontFace {
    return face == FrontFace::ccw ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
}

auto to_vk_stencil_op(StencilOp op) -> VkStencilOp {
    return static_cast<VkStencilOp>(op);
}

auto to_vk_blend_op(BlendOp op) -> VkBlendOp {
    switch (op) {
        case BlendOp::add: return VK_BLEND_OP_ADD;
        case BlendOp::subtract: return VK_BLEND_OP_SUBTRACT;
        case BlendOp::reverse_subtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
        case BlendOp::min: return VK_BLEND_OP_MIN;
        case BlendOp::max: return VK_BLEND_OP_MAX;
    }
    unreachable();
}

auto to_vk_blend_factor(BlendFactor factor) -> VkBlendFactor {
    switch (factor) {
        case BlendFactor::zero: return VK_BLEND_FACTOR_ZERO;
        case BlendFactor::one: return VK_BLEND_FACTOR_ONE;
        case BlendFactor::src: return VK_BLEND_FACTOR_SRC_COLOR;
        case BlendFactor::one_minus_src: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case BlendFactor::src_alpha: return VK_BLEND_FACTOR_SRC_ALPHA;
        case BlendFactor::one_minus_src_alpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::dst: return VK_BLEND_FACTOR_DST_COLOR;
        case BlendFactor::one_minus_dst: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case BlendFactor::dst_alpha: return VK_BLEND_FACTOR_DST_ALPHA;
        case BlendFactor::one_minus_dst_alpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case BlendFactor::src_alpha_saturated: return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        case BlendFactor::constant: return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case BlendFactor::one_minus_constant: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    }
    unreachable();
}

auto to_vk_color_write_mask(BitFlags<ColorWriteComponent> mask) -> VkColorComponentFlags {
    return static_cast<VkColorComponentFlags>(mask.raw_value());
}

}

GraphicsPipelineVulkan::GraphicsPipelineVulkan(Ref<DeviceVulkan> device, GraphicsPipelineDesc const& desc)
    : GraphicsPipeline(desc), device_(device)
{
    create_pipeline_layout(
        device_, set_layouts_, pipeline_layout_,
        desc_.bind_groups_layout, desc_.static_samplers, desc_.push_constants
    );
    if (!device_->use_descriptor_buffer() && !desc_.static_samplers.empty()) {
        immutable_samplers_set_ = device_->immutable_samplers_heap()->allocate_descriptor_raw(set_layouts_.back());
    }

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages{};
    std::vector<std::string> owned_shader_entries(5);
    {
        owned_shader_entries[0] = desc_.shaders.vertex.entry;
        shader_stages.push_back(to_vk_pipeline_shader_stage(
            desc.shaders.vertex, VK_SHADER_STAGE_VERTEX_BIT,
            owned_shader_entries[0].c_str()
        ));
    }
    if (desc_.shaders.tessellation_control) {
        owned_shader_entries[1] = desc_.shaders.tessellation_control.value().entry;
        shader_stages.push_back(to_vk_pipeline_shader_stage(
            desc.shaders.tessellation_control.value(), VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
            owned_shader_entries[1].c_str()
        ));
    }
    if (desc_.shaders.tessellation_evaluation) {
        owned_shader_entries[2] = desc_.shaders.tessellation_evaluation.value().entry;
        shader_stages.push_back(to_vk_pipeline_shader_stage(
            desc.shaders.tessellation_evaluation.value(), VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
            owned_shader_entries[2].c_str()
        ));
    }
    if (desc_.shaders.geometry) {
        owned_shader_entries[3] = desc_.shaders.geometry.value().entry;
        shader_stages.push_back(to_vk_pipeline_shader_stage(
            desc.shaders.geometry.value(), VK_SHADER_STAGE_GEOMETRY_BIT,
            owned_shader_entries[3].c_str()
        ));
    }
    {
        owned_shader_entries[4] = desc_.shaders.fragment.entry;
        shader_stages.push_back(to_vk_pipeline_shader_stage(
            desc.shaders.fragment, VK_SHADER_STAGE_FRAGMENT_BIT,
            owned_shader_entries[4].c_str()
        ));
    }

    std::vector<VkVertexInputAttributeDescription> vertex_input_attribute_descs{};
    std::vector<VkVertexInputBindingDescription> vertex_input_binding_descs(desc_.vertex_input_buffers.size());
    for (size_t i = 0; i < desc_.vertex_input_buffers.size(); i++) {
        auto const& input_buffer = desc_.vertex_input_buffers[i];
        vertex_input_binding_descs[i] = VkVertexInputBindingDescription{
            .binding = static_cast<uint32_t>(i),
            .stride = input_buffer.stride,
            .inputRate = input_buffer.per_instance ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX,
        };
        for (auto const& attribute : input_buffer.attributes) {
            uint32_t location = static_cast<uint32_t>(attribute.semantics);
            vertex_input_attribute_descs.push_back(VkVertexInputAttributeDescription{
                .location = location,
                .binding = static_cast<uint32_t>(i),
                .format = to_vk_format(attribute.format),
                .offset = attribute.offset,
            });
        }
    }
    VkPipelineVertexInputStateCreateInfo vertex_input_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .vertexBindingDescriptionCount = static_cast<uint32_t>(vertex_input_binding_descs.size()),
        .pVertexBindingDescriptions = vertex_input_binding_descs.data(),
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertex_input_attribute_descs.size()),
        .pVertexAttributeDescriptions = vertex_input_attribute_descs.data(),
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .topology = to_vk_primitive_topology(desc_.rasterization_state.topology),
        .primitiveRestartEnable = !is_primitive_list(desc_.rasterization_state.topology),
    };

    VkPipelineViewportStateCreateInfo viewport_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = nullptr,
        .scissorCount = 1,
        .pScissors = nullptr,
    };

    VkPipelineRasterizationStateCreateInfo rasterization_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = to_vk_polygon_mode(desc_.rasterization_state.polygon_mode),
        .cullMode = to_vk_cull_mode(desc_.rasterization_state.cull_mode),
        .frontFace = to_vk_front_face(desc_.rasterization_state.front_face),
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f,
        .lineWidth = 0.0f,
    };
    VkPipelineRasterizationConservativeStateCreateInfoEXT conservative_rasterization{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT,
        .pNext = nullptr,
        .flags = 0,
        .conservativeRasterizationMode = desc_.rasterization_state.conservative
            ? VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT
            : VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT,
        .extraPrimitiveOverestimationSize = 0.0f,
    };
    if (desc_.rasterization_state.conservative) {
        connect_vk_p_next_chain(&rasterization_state, &conservative_rasterization);
    }

    VkPipelineMultisampleStateCreateInfo multisample_state{
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

    VkPipelineDepthStencilStateCreateInfo depth_stencil_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthTestEnable = desc_.depth_stencil_state.depth_test,
        .depthWriteEnable = desc_.depth_stencil_state.depth_write,
        .depthCompareOp = to_vk_compare_op(desc_.depth_stencil_state.depth_compare_op),
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = desc_.depth_stencil_state.stencil_test,
        .front = VkStencilOpState{
            .failOp = to_vk_stencil_op(desc_.depth_stencil_state.stencil_front_face.fail_op),
            .passOp = to_vk_stencil_op(desc_.depth_stencil_state.stencil_front_face.pass_op),
            .depthFailOp = to_vk_stencil_op(desc_.depth_stencil_state.stencil_front_face.depth_fail_op),
            .compareOp = to_vk_compare_op(desc_.depth_stencil_state.stencil_front_face.compare_op),
            .compareMask = desc_.depth_stencil_state.stencil_compare_mask,
            .writeMask = desc_.depth_stencil_state.stencil_write_mask,
            .reference = desc_.depth_stencil_state.stencil_reference,
        },
        .back = VkStencilOpState{
            .failOp = to_vk_stencil_op(desc_.depth_stencil_state.stencil_back_face.fail_op),
            .passOp = to_vk_stencil_op(desc_.depth_stencil_state.stencil_back_face.pass_op),
            .depthFailOp = to_vk_stencil_op(desc_.depth_stencil_state.stencil_back_face.depth_fail_op),
            .compareOp = to_vk_compare_op(desc_.depth_stencil_state.stencil_back_face.compare_op),
            .compareMask = desc_.depth_stencil_state.stencil_compare_mask,
            .writeMask = desc_.depth_stencil_state.stencil_write_mask,
            .reference = desc_.depth_stencil_state.stencil_reference,
        },
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
    };
    auto depth_stencil_format_vk = to_vk_format(desc_.depth_stencil_state.format);

    std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachments(desc_.color_target_state.attachments.size());
    std::vector<VkFormat> color_formats_vk(desc_.color_target_state.attachments.size());
    for (size_t i = 0; i < color_blend_attachments.size(); i++) {
        auto const& attachment = desc_.color_target_state.attachments[i];
        color_blend_attachments[i] = VkPipelineColorBlendAttachmentState{
            .blendEnable = attachment.blend_enable,
            .srcColorBlendFactor = to_vk_blend_factor(attachment.src_blend_factor),
            .dstColorBlendFactor = to_vk_blend_factor(attachment.dst_blend_factor),
            .colorBlendOp = to_vk_blend_op(attachment.blend_op),
            .srcAlphaBlendFactor = to_vk_blend_factor(attachment.src_alpha_blend_factor),
            .dstAlphaBlendFactor = to_vk_blend_factor(attachment.dst_alpha_blend_factor),
            .alphaBlendOp = to_vk_blend_op(attachment.alpha_blend_op),
            .colorWriteMask = to_vk_color_write_mask(attachment.color_write_mask),
        };
        color_formats_vk[i] = to_vk_format(attachment.format);
    }
    VkPipelineColorBlendStateCreateInfo color_blend_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_NO_OP,
        .attachmentCount = static_cast<uint32_t>(color_blend_attachments.size()),
        .pAttachments = color_blend_attachments.data(),
        .blendConstants = {
            desc_.color_target_state.blend_constants.r,
            desc_.color_target_state.blend_constants.g,
            desc_.color_target_state.blend_constants.b,
            desc_.color_target_state.blend_constants.a,
        }
    };

    std::vector<VkDynamicState> dynamic_states{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamic_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data(),
    };

    VkPipelineCreateFlags flags = device_->use_descriptor_buffer()
        ? VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        : 0u;
    VkGraphicsPipelineCreateInfo pipeline_ci{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags,
        .stageCount = static_cast<uint32_t>(shader_stages.size()),
        .pStages = shader_stages.data(),
        .pVertexInputState = &vertex_input_state,
        .pInputAssemblyState = &input_assembly_state,
        .pTessellationState = nullptr,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterization_state,
        .pMultisampleState = &multisample_state,
        .pDepthStencilState = &depth_stencil_state,
        .pColorBlendState = &color_blend_state,
        .pDynamicState = &dynamic_state,
        .layout = pipeline_layout_,
        .renderPass = VK_NULL_HANDLE,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    };

    bool has_stencil = is_depth_stencil_format(desc.depth_stencil_state.format)
        && !is_depth_only_format(desc.depth_stencil_state.format);
    VkPipelineRenderingCreateInfo pipeline_rendering_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = nullptr,
        .viewMask = 0,
        .colorAttachmentCount = static_cast<uint32_t>(color_formats_vk.size()),
        .pColorAttachmentFormats = color_formats_vk.data(),
        .depthAttachmentFormat = depth_stencil_format_vk,
        .stencilAttachmentFormat = has_stencil ? depth_stencil_format_vk : VK_FORMAT_UNDEFINED,
    };
    connect_vk_p_next_chain(&pipeline_ci, &pipeline_rendering_ci);

    vkCreateGraphicsPipelines(device_->raw(), device_->pipeline_cache(), 1, &pipeline_ci, nullptr, &pipeline_);
}

GraphicsPipelineVulkan::~GraphicsPipelineVulkan() {
    if (pipeline_) {
        vkDestroyPipeline(device_->raw(), pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (!desc_.static_samplers.empty()) {
        vkDestroyDescriptorSetLayout(device_->raw(), set_layouts_.back(), nullptr);
    }
    set_layouts_.clear();
    if (pipeline_layout_) {
        vkDestroyPipelineLayout(device_->raw(), pipeline_layout_, nullptr);
        pipeline_layout_ = VK_NULL_HANDLE;
    }
}

auto GraphicsPipelineVulkan::push_constants_stages() const -> VkShaderStageFlags {
    return to_vk_shader_stages(desc_.push_constants.visibility);
}

auto GraphicsPipelineVulkan::static_samplers_set() const -> uint32_t {
    if (desc_.static_samplers.empty()) {
        return ~0u;
    } else {
        return set_layouts_.size() - 1;
    }
}


ComputePipelineVulkan::ComputePipelineVulkan(Ref<DeviceVulkan> device, ComputePipelineDesc const& desc)
    : ComputePipeline(desc), device_(device)
{
    create_pipeline_layout(
        device_, set_layouts_, pipeline_layout_,
        desc_.bind_groups_layout, desc_.static_samplers, desc_.push_constants
    );
    if (!device_->use_descriptor_buffer() && !desc_.static_samplers.empty()) {
        immutable_samplers_set_ = device_->immutable_samplers_heap()->allocate_descriptor_raw(set_layouts_.back());
    }

    auto owned_entry = std::string{desc_.compute.entry};
    VkPipelineCreateFlags flags = device_->use_descriptor_buffer()
        ? VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        : 0u;
    VkComputePipelineCreateInfo pipeline_ci{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags,
        .stage = to_vk_pipeline_shader_stage(desc_.compute, VK_SHADER_STAGE_COMPUTE_BIT, owned_entry.c_str()),
        .layout = pipeline_layout_,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    };
    vkCreateComputePipelines(device->raw(), device_->pipeline_cache(), 1, &pipeline_ci, nullptr, &pipeline_);
}

ComputePipelineVulkan::~ComputePipelineVulkan() {
    if (pipeline_) {
        vkDestroyPipeline(device_->raw(), pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (!desc_.static_samplers.empty()) {
        vkDestroyDescriptorSetLayout(device_->raw(), set_layouts_.back(), nullptr);
    }
    set_layouts_.clear();
    if (pipeline_layout_) {
        vkDestroyPipelineLayout(device_->raw(), pipeline_layout_, nullptr);
        pipeline_layout_ = VK_NULL_HANDLE;
    }
}

auto ComputePipelineVulkan::push_constants_stages() const -> VkShaderStageFlags {
    return to_vk_shader_stages(desc_.push_constants.visibility);
}

auto ComputePipelineVulkan::static_samplers_set() const -> uint32_t {
    if (desc_.static_samplers.empty()) {
        return ~0u;
    } else {
        return set_layouts_.size() - 1;
    }
}


RaytracingPipelineVulkan::RaytracingPipelineVulkan(Ref<DeviceVulkan> device, RaytracingPipelineDesc const& desc)
    : RaytracingPipeline(desc), device_(device)
{
    create_pipeline_layout(
        device_, set_layouts_, pipeline_layout_,
        desc_.bind_groups_layout, desc_.static_samplers, desc_.push_constants
    );
    if (!device_->use_descriptor_buffer() && !desc_.static_samplers.empty()) {
        immutable_samplers_set_ = device_->immutable_samplers_heap()->allocate_descriptor_raw(set_layouts_.back());
    }

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_groups;
    std::list<std::string> owned_entries;

    {
        auto& group = shader_groups.emplace_back();
        group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        group.pNext = nullptr;
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        group.generalShader = shader_stages.size();
        group.closestHitShader = VK_SHADER_UNUSED_KHR;
        group.anyHitShader = VK_SHADER_UNUSED_KHR;
        group.intersectionShader = VK_SHADER_UNUSED_KHR;

        auto& entry = owned_entries.emplace_back(desc_.shaders.raygen.shader.entry);
        shader_stages.push_back(
            to_vk_pipeline_shader_stage(desc_.shaders.raygen.shader, VK_SHADER_STAGE_RAYGEN_BIT_KHR, entry.c_str())
        );
    }
    for (auto& miss : desc_.shaders.miss) {
        auto& group = shader_groups.emplace_back();
        group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        group.pNext = nullptr;
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        group.generalShader = shader_stages.size();
        group.closestHitShader = VK_SHADER_UNUSED_KHR;
        group.anyHitShader = VK_SHADER_UNUSED_KHR;
        group.intersectionShader = VK_SHADER_UNUSED_KHR;

        auto& entry = owned_entries.emplace_back(miss.shader.entry);
        shader_stages.push_back(
            to_vk_pipeline_shader_stage(miss.shader, VK_SHADER_STAGE_MISS_BIT_KHR, entry.c_str())
        );
    }
    for (auto& hit : desc_.shaders.hit_group) {
        auto& group = shader_groups.emplace_back();
        group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        group.pNext = nullptr;
        group.type = hit.intersection.has_value()
            ? VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR
            : VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        group.generalShader = VK_SHADER_UNUSED_KHR;
        group.closestHitShader = VK_SHADER_UNUSED_KHR;
        group.anyHitShader = VK_SHADER_UNUSED_KHR;
        group.intersectionShader = VK_SHADER_UNUSED_KHR;

        if (auto& shader = hit.closest_hit; shader) {
            group.closestHitShader = shader_stages.size();
            auto& entry = owned_entries.emplace_back(shader.value().entry);
            shader_stages.push_back(
                to_vk_pipeline_shader_stage(shader.value(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, entry.c_str())
            );
        }
        if (auto& shader = hit.any_hit; shader) {
            group.anyHitShader = shader_stages.size();
            auto& entry = owned_entries.emplace_back(shader.value().entry);
            shader_stages.push_back(
                to_vk_pipeline_shader_stage(shader.value(), VK_SHADER_STAGE_ANY_HIT_BIT_KHR, entry.c_str())
            );
        }
        if (auto& shader = hit.intersection; shader) {
            group.intersectionShader = shader_stages.size();
            auto& entry = owned_entries.emplace_back(shader.value().entry);
            shader_stages.push_back(
                to_vk_pipeline_shader_stage(shader.value(), VK_SHADER_STAGE_INTERSECTION_BIT_KHR, entry.c_str())
            );
        }
    }
    for (auto& callable : desc_.shaders.callable) {
        auto& group = shader_groups.emplace_back();
        group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        group.pNext = nullptr;
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        group.generalShader = shader_stages.size();
        group.closestHitShader = VK_SHADER_UNUSED_KHR;
        group.anyHitShader = VK_SHADER_UNUSED_KHR;
        group.intersectionShader = VK_SHADER_UNUSED_KHR;

        auto& entry = owned_entries.emplace_back(callable.shader.entry);
        shader_stages.push_back(
            to_vk_pipeline_shader_stage(callable.shader, VK_SHADER_STAGE_CALLABLE_BIT_KHR, entry.c_str())
        );
    }

    VkRayTracingPipelineInterfaceCreateInfoKHR pipeline_interface_ci{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_INTERFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .maxPipelineRayPayloadSize = desc_.state.max_ray_payload_size,
        .maxPipelineRayHitAttributeSize = desc_.state.max_hit_attribute_size,
    };
    VkPipelineCreateFlags flags = (device_->use_descriptor_buffer() ? VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT : 0u)
        | (desc_.state.skip_procedural ? VK_PIPELINE_CREATE_RAY_TRACING_SKIP_AABBS_BIT_KHR : 0u);
    VkRayTracingPipelineCreateInfoKHR pipeline_ci{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = flags,
        .stageCount = static_cast<uint32_t>(shader_stages.size()),
        .pStages = shader_stages.data(),
        .groupCount = static_cast<uint32_t>(shader_groups.size()),
        .pGroups = shader_groups.data(),
        .maxPipelineRayRecursionDepth = desc_.state.max_recursive_depth,
        .pLibraryInfo = nullptr,
        .pLibraryInterface = &pipeline_interface_ci,
        .pDynamicState = nullptr,
        .layout = pipeline_layout_,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    };
    vkCreateRayTracingPipelinesKHR(
        device_->raw(), VK_NULL_HANDLE, device_->pipeline_cache(), 1, &pipeline_ci, nullptr, &pipeline_
    );
}

RaytracingPipelineVulkan::~RaytracingPipelineVulkan() {
    if (pipeline_) {
        vkDestroyPipeline(device_->raw(), pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (!desc_.static_samplers.empty()) {
        vkDestroyDescriptorSetLayout(device_->raw(), set_layouts_.back(), nullptr);
    }
    set_layouts_.clear();
    if (pipeline_layout_) {
        vkDestroyPipelineLayout(device_->raw(), pipeline_layout_, nullptr);
        pipeline_layout_ = VK_NULL_HANDLE;
    }
}

auto RaytracingPipelineVulkan::get_shader_binding_table_sizes() const -> RaytracingShaderBindingTableSizes {
    auto requirements = device_->raytracing_shader_binding_table_requirements();
    return RaytracingShaderBindingTableSizes{
        .raygen_size = aligned_size<uint32_t>(
            requirements.handle_size + desc_.shader_record_sizes.raygen, requirements.handle_alignment
        ),
        .miss_stride = aligned_size<uint32_t>(
            requirements.handle_size + desc_.shader_record_sizes.miss, requirements.handle_alignment
        ),
        .hit_group_stride = aligned_size<uint32_t>(
            requirements.handle_size + desc_.shader_record_sizes.hit_group, requirements.handle_alignment
        ),
        .callable_stride = aligned_size<uint32_t>(
            requirements.handle_size + desc_.shader_record_sizes.callable, requirements.handle_alignment
        ),
    };
}

auto RaytracingPipelineVulkan::get_shader_handle(
    RaytracingShaderBindingTableType type, uint32_t from_index, uint32_t count, void* dst_data
) const -> void {
    switch (type) {
        case RaytracingShaderBindingTableType::raygen:
            count = from_index == 0 ? 1 : 0;
            break;
        case RaytracingShaderBindingTableType::miss:
            count = std::min<uint32_t>(from_index + count, desc_.shaders.miss.size())
                - std::min<uint32_t>(from_index, desc_.shaders.miss.size());
            from_index += 1;
            break;
        case RaytracingShaderBindingTableType::hit_group:
            count = std::min<uint32_t>(from_index + count, desc_.shaders.hit_group.size())
                - std::min<uint32_t>(from_index, desc_.shaders.hit_group.size());
            from_index += 1 + desc_.shaders.miss.size();
            break;
        case RaytracingShaderBindingTableType::callable:
            count = std::min<uint32_t>(from_index + count, desc_.shaders.callable.size())
                - std::min<uint32_t>(from_index, desc_.shaders.callable.size());
            from_index += 1 + desc_.shaders.miss.size() + desc_.shaders.hit_group.size();
            break;
    }
    if (count > 0) {
        auto handle_size = device_->raytracing_shader_binding_table_requirements().handle_size;
        vkGetRayTracingShaderGroupHandlesKHR(device_->raw(), pipeline_, from_index, count, count * handle_size, dst_data);
    }
}

auto RaytracingPipelineVulkan::push_constants_stages() const -> VkShaderStageFlags {
    return to_vk_shader_stages(desc_.push_constants.visibility);
}

auto RaytracingPipelineVulkan::static_samplers_set() const -> uint32_t {
    if (desc_.static_samplers.empty()) {
        return ~0u;
    } else {
        return set_layouts_.size() - 1;
    }
}

}
