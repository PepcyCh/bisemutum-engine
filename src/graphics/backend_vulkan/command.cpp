#include "command.hpp"

#include "core/logger.hpp"

#include "utils.hpp"
#include "device.hpp"
#include "resource.hpp"
#include "pipeline.hpp"
#include "context.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

namespace {

VkDebugUtilsLabelEXT ToVkDebugLabel(const CommandLabel &label) {
    return VkDebugUtilsLabelEXT {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        .pNext = nullptr,
        .pLabelName = label.label.data(),
        .color = {label.color.r, label.color.g, label.color.b, 1.0f},
    };
}

Vec<VkBufferCopy> ToVkBufferCopy(Ref<BufferVulkan> src_buffer_vk, Ref<BufferVulkan> dst_buffer_vk,
    Span<BufferCopyDesc> regions) {
    Vec<VkBufferCopy> regions_vk(regions.size());
    for (size_t i = 0; i < regions.size(); i++) {
        uint64_t length = std::min({ regions[i].length, src_buffer_vk->Size() - regions[i].src_offset,
            dst_buffer_vk->Size() - regions[i].dst_offset });
        regions_vk[i] = VkBufferCopy {
            .srcOffset = regions[i].src_offset,
            .dstOffset = regions[i].dst_offset,
            .size = length,
        };
    }
    return regions_vk;
}

Vec<VkImageCopy> ToVkImageCopy(Ref<TextureVulkan> src_texture_vk, Ref<TextureVulkan> dst_texture_vk,
    Span<TextureCopyDesc> regions) {
    Vec<VkImageCopy> regions_vk(regions.size());
    for (size_t i = 0; i < regions.size(); i++) {
        uint32_t src_base_depth, src_base_layer;
        src_texture_vk->GetDepthAndLayer(regions[i].src_offset.z, src_base_depth, src_base_layer, 0);
        uint32_t dst_base_depth, dst_base_layer;
        dst_texture_vk->GetDepthAndLayer(regions[i].dst_offset.z, dst_base_depth, dst_base_layer, 0);
        uint32_t depth, layers;
        src_texture_vk->GetDepthAndLayer(regions[i].extent.depth_or_layers, depth, layers);
        regions_vk[i] = VkImageCopy {
            .srcSubresource = VkImageSubresourceLayers {
                .aspectMask = src_texture_vk->GetAspect(),
                .mipLevel = regions[i].src_level,
                .baseArrayLayer = src_base_layer,
                .layerCount = layers,
            },
            .srcOffset = VkOffset3D {
                .x = static_cast<int>(regions[i].src_offset.x),
                .y = static_cast<int>(regions[i].src_offset.y),
                .z = static_cast<int>(src_base_depth),
            },
            .dstSubresource = VkImageSubresourceLayers {
                .aspectMask = dst_texture_vk->GetAspect(),
                .mipLevel = regions[i].dst_level,
                .baseArrayLayer = dst_base_layer,
                .layerCount = layers,
            },
            .dstOffset = VkOffset3D {
                .x = static_cast<int>(regions[i].dst_offset.x),
                .y = static_cast<int>(regions[i].dst_offset.y),
                .z = static_cast<int>(dst_base_depth),
            },
            .extent = VkExtent3D {
                .width = regions[i].extent.width,
                .height = regions[i].extent.height,
                .depth = depth,
            }
        };
    }
    return regions_vk;
}

Vec<VkBufferImageCopy> ToVkBufferImageCopy(Ref<TextureVulkan> texture_vk, Span<BufferTextureCopyDesc> regions) {
    Vec<VkBufferImageCopy> regions_vk(regions.size());
    for (size_t i = 0; i < regions.size(); i++) {
        uint32_t base_depth, base_layer;
        texture_vk->GetDepthAndLayer(regions[i].texture_offset.z, base_depth, base_layer, 0);
        uint32_t depth, layers;
        texture_vk->GetDepthAndLayer(regions[i].texture_extent.depth_or_layers, depth, layers);
        regions_vk[i] = VkBufferImageCopy {
            .bufferOffset = regions[i].buffer_offset,
            .bufferRowLength = regions[i].buffer_bytes_per_row / FormatTexelSize(texture_vk->Desc().format),
            .bufferImageHeight = regions[i].buffer_rows_per_texture,
            .imageSubresource = VkImageSubresourceLayers {
                .aspectMask = texture_vk->GetAspect(),
                .mipLevel = regions[i].texture_level,
                .baseArrayLayer = base_layer,
                .layerCount = layers,
            },
            .imageOffset = VkOffset3D {
                .x = static_cast<int>(regions[i].texture_offset.x),
                .y = static_cast<int>(regions[i].texture_offset.y),
                .z = static_cast<int>(base_depth),
            },
            .imageExtent = VkExtent3D {
                .width = regions[i].texture_extent.width,
                .height = regions[i].texture_extent.height,
                .depth = depth,
            },
        };
    }
    return regions_vk;
}

void ToVkBufferAccessType(BitFlags<ResourceAccessType> type, BitFlags<ResourceAccessStage> stage,
    VkAccessFlags2 &type_vk, VkPipelineStageFlags2 &stage_vk) {
    type_vk = 0;
    stage_vk = 0;
    if (type.Contains(ResourceAccessType::eVertexBufferRead)) {
        type_vk |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
        stage_vk |= VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
    }
    if (type.Contains(ResourceAccessType::eIndexBufferRead)) {
        type_vk |= VK_ACCESS_2_INDEX_READ_BIT;
        stage_vk |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
    }
    if (type.Contains(ResourceAccessType::eIndirectRead)) {
        type_vk |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
        stage_vk |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
    }
    if (type.Contains(ResourceAccessType::eUniformBufferRead)) {
        type_vk |= VK_ACCESS_2_UNIFORM_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT;
        if (stage.Contains(ResourceAccessStage::eGraphicsShader)) {
            stage_vk |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
        }
        if (stage.Contains(ResourceAccessStage::eComputeShader)) {
            stage_vk |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        }
    }
    if (type.Contains(ResourceAccessType::eStorageResourceRead)) {
        type_vk |= VK_ACCESS_2_SHADER_READ_BIT;
        if (stage.Contains(ResourceAccessStage::eGraphicsShader)) {
            stage_vk |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
        }
        if (stage.Contains(ResourceAccessStage::eComputeShader)) {
            stage_vk |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        }
    }
    if (type.Contains(ResourceAccessType::eStorageResourceWrite)) {
        type_vk |= VK_ACCESS_2_SHADER_WRITE_BIT;
        if (stage.Contains(ResourceAccessStage::eGraphicsShader)) {
            stage_vk |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
        }
        if (stage.Contains(ResourceAccessStage::eComputeShader)) {
            stage_vk |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        }
    }
    if (type.Contains(ResourceAccessType::eTransferRead)) {
        type_vk |= VK_ACCESS_2_TRANSFER_READ_BIT;
        stage_vk |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    }
    if (type.Contains(ResourceAccessType::eTransferWrite)) {
        type_vk |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
        stage_vk |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    }
}

void ToVkImageAccessType(BitFlags<ResourceAccessType> type, BitFlags<ResourceAccessStage> stage, bool is_depth_stencil,
    VkAccessFlags2 &type_vk, VkPipelineStageFlags2 &stage_vk, VkImageLayout &layout_vk) {
    type_vk = 0;
    stage_vk = 0;
    layout_vk = VK_IMAGE_LAYOUT_UNDEFINED;
    if (type.Contains(ResourceAccessType::eSampledTextureRead)) {
        type_vk |= VK_ACCESS_2_SHADER_READ_BIT;
        if (stage.Contains(ResourceAccessStage::eGraphicsShader)) {
            stage_vk |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
        }
        if (stage.Contains(ResourceAccessStage::eComputeShader)) {
            stage_vk |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        }
        layout_vk = is_depth_stencil ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
            : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    if (type.Contains(ResourceAccessType::eStorageResourceRead)) {
        type_vk |= VK_ACCESS_2_SHADER_READ_BIT;
        if (stage.Contains(ResourceAccessStage::eGraphicsShader)) {
            stage_vk |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
        }
        if (stage.Contains(ResourceAccessStage::eComputeShader)) {
            stage_vk |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        }
        layout_vk = VK_IMAGE_LAYOUT_GENERAL;
    }
    if (type.Contains(ResourceAccessType::eStorageResourceWrite)) {
        type_vk |= VK_ACCESS_2_SHADER_WRITE_BIT;
        if (stage.Contains(ResourceAccessStage::eGraphicsShader)) {
            stage_vk |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
        }
        if (stage.Contains(ResourceAccessStage::eComputeShader)) {
            stage_vk |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        }
        layout_vk = VK_IMAGE_LAYOUT_GENERAL;
    }
    if (type.Contains(ResourceAccessType::eRenderAttachmentRead)) {
        if (stage.Contains(ResourceAccessStage::eColorAttachment)) {
            type_vk |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
            stage_vk |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            layout_vk = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
        if (stage.Contains(ResourceAccessStage::eDepthStencilAttachment)) {
            type_vk |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            stage_vk |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            layout_vk = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        }
    }
    if (type.Contains(ResourceAccessType::eRenderAttachmentWrite)) {
        if (stage.Contains(ResourceAccessStage::eColorAttachment)) {
            type_vk |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            stage_vk |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            layout_vk = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
        if (stage.Contains(ResourceAccessStage::eDepthStencilAttachment)) {
            type_vk |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            stage_vk |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            layout_vk = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }
    }
    if (type.Contains(ResourceAccessType::eTransferRead)) {
        type_vk |= VK_ACCESS_2_TRANSFER_READ_BIT;
        if (stage.Contains(ResourceAccessStage::eTransfer)) {
            stage_vk |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        }
        if (stage.Contains(ResourceAccessStage::eResolve)) {
            stage_vk |= VK_PIPELINE_STAGE_2_RESOLVE_BIT;
        }
        layout_vk = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }
    if (type.Contains(ResourceAccessType::eTransferWrite)) {
        type_vk |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
        if (stage.Contains(ResourceAccessStage::eTransfer)) {
            stage_vk |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        }
        if (stage.Contains(ResourceAccessStage::eResolve)) {
            stage_vk |= VK_PIPELINE_STAGE_2_RESOLVE_BIT;
        }
        layout_vk = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    }
    if (type.Contains(ResourceAccessType::ePresent)) {
        stage_vk |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        layout_vk = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }
}

}

CommandBufferVulkan::CommandBufferVulkan(Ref<DeviceVulkan> device, VkCommandBuffer cmd_buffer)
    : device_(device), cmd_buffer_(cmd_buffer) {}

CommandEncoderVulkan::CommandEncoderVulkan(Ref<DeviceVulkan> device, Ref<FrameContextVulkan> context,
    VkCommandBuffer cmd_buffer) : device_(device), context_(context), cmd_buffer_(cmd_buffer) {}

CommandEncoderVulkan::~CommandEncoderVulkan() {
    BI_ASSERT(cmd_buffer_ == VK_NULL_HANDLE);
}

Ptr<CommandBuffer> CommandEncoderVulkan::Finish() {
    vkEndCommandBuffer(cmd_buffer_);
    auto cmd_buffer = Ptr<CommandBufferVulkan>::Make(device_, cmd_buffer_);
    cmd_buffer_ = VK_NULL_HANDLE;
    return cmd_buffer;
}

void CommandEncoderVulkan::PushLabel(const CommandLabel &label) {
    auto label_vk = ToVkDebugLabel(label);
    vkCmdBeginDebugUtilsLabelEXT(cmd_buffer_, &label_vk);
}

void CommandEncoderVulkan::PopLabel() {
    vkCmdEndDebugUtilsLabelEXT(cmd_buffer_);
}

void CommandEncoderVulkan::CopyBufferToBuffer(Ref<Buffer> src_buffer, Ref<Buffer> dst_buffer,
    Span<BufferCopyDesc> regions) {
    auto src_buffer_vk = src_buffer.CastTo<BufferVulkan>();
    auto dst_buffer_vk = dst_buffer.CastTo<BufferVulkan>();
    auto regions_vk = ToVkBufferCopy(src_buffer_vk, dst_buffer_vk, regions);
    vkCmdCopyBuffer(cmd_buffer_, src_buffer_vk->Raw(), dst_buffer_vk->Raw(), regions_vk.size(), regions_vk.data());
}

void CommandEncoderVulkan::CopyTextureToTexture(Ref<Texture> src_texture, Ref<Texture> dst_texture,
    Span<TextureCopyDesc> regions) {
    auto src_texture_vk = src_texture.CastTo<TextureVulkan>();
    auto dst_texture_vk = dst_texture.CastTo<TextureVulkan>();
    auto regions_vk = ToVkImageCopy(src_texture_vk, dst_texture_vk, regions);
    vkCmdCopyImage(cmd_buffer_, src_texture_vk->Raw(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst_texture_vk->Raw(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regions_vk.size(), regions_vk.data());
}

void CommandEncoderVulkan::CopyBufferToTexture(Ref<Buffer> src_buffer, Ref<Texture> dst_texture,
    Span<BufferTextureCopyDesc> regions) {
    auto src_buffer_vk = src_buffer.CastTo<BufferVulkan>();
    auto dst_texture_vk = dst_texture.CastTo<TextureVulkan>();
    auto regions_vk = ToVkBufferImageCopy(dst_texture_vk, regions);
    vkCmdCopyBufferToImage(cmd_buffer_, src_buffer_vk->Raw(), dst_texture_vk->Raw(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regions_vk.size(), regions_vk.data());
}

void CommandEncoderVulkan::CopyTextureToBuffer(Ref<Texture> src_texture, Ref<Buffer> dst_buffer,
    Span<BufferTextureCopyDesc> regions) {
    auto src_texture_vk = src_texture.CastTo<TextureVulkan>();
    auto dst_buffer_vk = dst_buffer.CastTo<BufferVulkan>();
    auto regions_vk = ToVkBufferImageCopy(src_texture_vk, regions);
    vkCmdCopyImageToBuffer(cmd_buffer_, src_texture_vk->Raw(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dst_buffer_vk->Raw(), regions_vk.size(), regions_vk.data());
}

void CommandEncoderVulkan::ResourceBarrier(Span<BufferBarrier> buffer_barriers, Span<TextureBarrier> texture_barriers) {
    Vec<VkBufferMemoryBarrier2> buffer_barriers_vk(buffer_barriers.Size());
    for (size_t i = 0; i < buffer_barriers.Size(); i++) {
        const auto &barrier = buffer_barriers[i];
        buffer_barriers_vk[i] = VkBufferMemoryBarrier2 {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcQueueFamilyIndex = barrier.src_queue ? static_cast<QueueVulkan *>(barrier.src_queue)->RawFamilyIndex()
                : VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = barrier.dst_queue ? static_cast<QueueVulkan *>(barrier.dst_queue)->RawFamilyIndex()
                : VK_QUEUE_FAMILY_IGNORED,
            .buffer = barrier.buffer.CastTo<BufferVulkan>()->Raw(),
            .offset = 0,
            .size = VK_WHOLE_SIZE,
        };
        ToVkBufferAccessType(barrier.src_access_type, barrier.src_access_stage,
            buffer_barriers_vk[i].srcAccessMask, buffer_barriers_vk[i].srcStageMask);
        ToVkBufferAccessType(barrier.dst_access_type, barrier.dst_access_stage,
            buffer_barriers_vk[i].dstAccessMask, buffer_barriers_vk[i].dstStageMask);
    }
    Vec<VkImageMemoryBarrier2> texture_barriers_vk(texture_barriers.Size());
    for (size_t i = 0; i < texture_barriers.Size(); i++) {
        const auto &barrier = texture_barriers[i];
        const auto texture_vk = barrier.texture.texture.CastTo<TextureVulkan>();
        texture_barriers_vk[i] = VkImageMemoryBarrier2 {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcQueueFamilyIndex = barrier.src_queue ? static_cast<QueueVulkan *>(barrier.src_queue)->RawFamilyIndex()
                : VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = barrier.dst_queue ? static_cast<QueueVulkan *>(barrier.dst_queue)->RawFamilyIndex()
                : VK_QUEUE_FAMILY_IGNORED,
            .image = texture_vk->Raw(),
            .subresourceRange = VkImageSubresourceRange {
                .aspectMask = texture_vk->GetAspect(),
                .baseMipLevel = barrier.texture.base_level,
                .levelCount = barrier.texture.levels,
                .baseArrayLayer = barrier.texture.base_layer,
                .layerCount = barrier.texture.layers,
            },
        };
        bool is_depth_stencil = IsDepthStencilFormat(texture_vk->Desc().format);
        ToVkImageAccessType(barrier.src_access_type, barrier.src_access_stage,
            is_depth_stencil, texture_barriers_vk[i].srcAccessMask,
            texture_barriers_vk[i].srcStageMask, texture_barriers_vk[i].oldLayout);
        ToVkImageAccessType(barrier.dst_access_type, barrier.dst_access_stage,
            is_depth_stencil, texture_barriers_vk[i].dstAccessMask,
            texture_barriers_vk[i].dstStageMask, texture_barriers_vk[i].newLayout);
    }

    VkDependencyInfo dep_info {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .dependencyFlags = 0,
        .memoryBarrierCount = 0,
        .pMemoryBarriers = nullptr,
        .bufferMemoryBarrierCount = static_cast<uint32_t>(buffer_barriers_vk.size()),
        .pBufferMemoryBarriers = buffer_barriers_vk.data(),
        .imageMemoryBarrierCount = static_cast<uint32_t>(texture_barriers_vk.size()),
        .pImageMemoryBarriers = texture_barriers_vk.data(),
    };
#if BISMUTH_VULKAN_VERSION_MINOR < 3
    vkCmdPipelineBarrier2KHR(cmd_buffer_, &dep_info);
#else
    vkCmdPipelineBarrier2(cmd_buffer_, &dep_info);
#endif
}

Ptr<RenderCommandEncoder> CommandEncoderVulkan::BeginRenderPass(const CommandLabel &label,
    const RenderTargetDesc &desc) {
    if (!label.label.empty()) {
        PushLabel(label);
    }
    
    auto render_encoder = Ptr<RenderCommandEncoderVulkan>::Make(device_, desc, RefThis(), label.label);
    cmd_buffer_ = VK_NULL_HANDLE;
    return render_encoder;
}

Ptr<ComputeCommandEncoder> CommandEncoderVulkan::BeginComputePass(const CommandLabel &label) {
    if (!label.label.empty()) {
        PushLabel(label);
    }
    auto compute_encoder = Ptr<ComputeCommandEncoderVulkan>::Make(device_, RefThis(), label.label);
    cmd_buffer_ = VK_NULL_HANDLE;
    return compute_encoder;
}


RenderCommandEncoderVulkan::RenderCommandEncoderVulkan(Ref<DeviceVulkan> device, const RenderTargetDesc &desc,
    Ref<CommandEncoderVulkan> base_encoder, const std::string &label)
    : device_(device), base_encoder_(base_encoder), label_(label) {
    cmd_buffer_ = base_encoder->cmd_buffer_;

    BI_ASSERT(desc.colors.size() > 0 || desc.depth_stencil.has_value());

    Extent3D extent;

    Vec<VkRenderingAttachmentInfoKHR> color_attachments_vk(desc.colors.size());
    for (size_t i = 0; i < desc.colors.size(); i++) {
        auto texture_vk = desc.colors[i].texture.texture.CastTo<TextureVulkan>();
        const auto &clear_color = desc.colors[i].clear_color;
        extent = texture_vk->Desc().extent;

        color_attachments_vk[i] = VkRenderingAttachmentInfoKHR {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
            .pNext = nullptr,
            .imageView = texture_vk->GetView(TextureViewVulkanDesc {
                .type = VK_IMAGE_VIEW_TYPE_2D,
                .format = texture_vk->RawFormat(),
                .base_layer = desc.colors[i].texture.base_layer,
                .layers = desc.colors[i].texture.layers,
                .base_level = desc.colors[i].texture.base_level,
                .levels = desc.colors[i].texture.levels,
            }),
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            // TODO - support resolve
            .resolveMode = VK_RESOLVE_MODE_NONE,
            .resolveImageView = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp = desc.colors[i].clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = desc.colors[i].store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .clearValue = VkClearValue {
                .color = VkClearColorValue {
                    .float32 = { clear_color.r, clear_color.g, clear_color.b, clear_color.a },
                },
            }
        };
    }
    
    bool has_depth_stencil = desc.depth_stencil.has_value();
    bool has_stencil = false;
    VkRenderingAttachmentInfoKHR depth_stencil_attachment_vk;
    if (has_depth_stencil) {
        const auto &depth_stencil = desc.depth_stencil.value();
        auto texture_vk = depth_stencil.texture.texture.CastTo<TextureVulkan>();
        has_stencil = IsDepthStencilFormat(texture_vk->Desc().format);
        extent = texture_vk->Desc().extent;

        depth_stencil_attachment_vk = VkRenderingAttachmentInfoKHR {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
            .pNext = nullptr,
            .imageView = texture_vk->GetView(TextureViewVulkanDesc {
                .type = VK_IMAGE_VIEW_TYPE_2D,
                .format = texture_vk->RawFormat(),
                .base_layer = depth_stencil.texture.base_layer,
                .layers = depth_stencil.texture.layers,
                .base_level = depth_stencil.texture.base_level,
                .levels = depth_stencil.texture.levels,
            }),
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .resolveMode = VK_RESOLVE_MODE_NONE,
            .resolveImageView = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp = depth_stencil.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = depth_stencil.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .clearValue = VkClearValue {
            .depthStencil = VkClearDepthStencilValue {
                    .depth = depth_stencil.clear_depth,
                    .stencil = depth_stencil.clear_stencil,
                },
            },
        };
    }

    VkRenderingInfoKHR rendering_info {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .renderArea = {
            .offset = { 0, 0 },
            .extent = { extent.width, extent.height }
        },
        .layerCount = 1,
        .viewMask = 0,
        .colorAttachmentCount = static_cast<uint32_t>(color_attachments_vk.size()),
        .pColorAttachments = color_attachments_vk.data(),
        .pDepthAttachment = has_depth_stencil ? &depth_stencil_attachment_vk : nullptr,
        .pStencilAttachment = has_stencil ? &depth_stencil_attachment_vk : nullptr,
    };
#if BISMUTH_VULKAN_VERSION_MINOR < 3
    vkCmdBeginRenderingKHR(cmd_buffer_, &rendering_info);
#else
    vkCmdBeginRendering(cmd_buffer_, &rendering_info);
#endif
}

RenderCommandEncoderVulkan::~RenderCommandEncoderVulkan() {
#if BISMUTH_VULKAN_VERSION_MINOR < 3
    vkCmdEndRenderingKHR(cmd_buffer_);
#else
    vkCmdEndRendering(cmd_buffer_);
#endif

    if (!label_.empty()) {
        PopLabel();
    }
    base_encoder_->cmd_buffer_ = cmd_buffer_;
}

Ptr<CommandBuffer> RenderCommandEncoderVulkan::Finish() {
    vkEndCommandBuffer(cmd_buffer_);
    auto cmd_buffer = Ptr<CommandBufferVulkan>::Make(device_, cmd_buffer_);
    cmd_buffer_ = VK_NULL_HANDLE;
    return cmd_buffer;
}

void RenderCommandEncoderVulkan::PushLabel(const CommandLabel &label) {
    auto label_vk = ToVkDebugLabel(label);
    vkCmdBeginDebugUtilsLabelEXT(cmd_buffer_, &label_vk);
}

void RenderCommandEncoderVulkan::PopLabel() {
    vkCmdEndDebugUtilsLabelEXT(cmd_buffer_);
}

void RenderCommandEncoderVulkan::SetPipeline(Ref<RenderPipeline> pipeline) {
    curr_pipeline_ = pipeline.CastTo<RenderPipelineVulkan>().Get();
    vkCmdBindPipeline(cmd_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, curr_pipeline_->RawPipeline());

    // descriptor_sets_.resize(curr_pipeline_->ShaderInfo().bindings.bindings.size(), nullptr);
    // for (size_t set = 0; set < descriptor_sets_.size(); set++) {
    //     descriptor_sets_[set] = base_encoder_->context_->GetDescriptorSet(curr_pipeline_, set);
    //     raw_descriptor_sets_[set] = descriptor_sets_[set]->Raw();
    // }
}

void RenderCommandEncoderVulkan::BindShaderParams(uint32_t set_index, const ShaderParams &values) {
    BI_ASSERT_MSG(curr_pipeline_, "Call RenderCommandEncoder::BindShaderParams() without setting pipeline");

    VkDescriptorSet descriptor_set = base_encoder_->context_->GetDescriptorSet(curr_pipeline_->RawSetLayout(set_index),
        curr_pipeline_->Desc().layout.sets_layout[set_index], values);

    vkCmdBindDescriptorSets(cmd_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, curr_pipeline_->RawPipelineLayout(),
        set_index, 1, &descriptor_set, 0, nullptr);
}

void RenderCommandEncoderVulkan::PushConstants(const void *data, uint32_t size, uint32_t offset) {
    BI_ASSERT_MSG(curr_pipeline_, "Call RenderCommandEncoder::PushConstants() without setting pipeline");

    vkCmdPushConstants(cmd_buffer_, curr_pipeline_->RawPipelineLayout(), VK_SHADER_STAGE_ALL_GRAPHICS,
        offset, size, data);
}

void RenderCommandEncoderVulkan::SetViewports(Span<Viewport> viewports) {
    Vec<VkViewport> viewports_vk(viewports.Size());
    for (size_t i = 0; i < viewports_vk.size(); i++) {
        // inverse Vulkan viewport to make result the same as D3D12
        viewports_vk[i] = VkViewport {
            .x = viewports[i].x,
            .y = viewports[i].height - viewports[i].y,
            .width = viewports[i].width,
            .height = -viewports[i].height,
            .minDepth = viewports[i].min_depth,
            .maxDepth = viewports[i].max_depth,
        };
    }
    vkCmdSetViewport(cmd_buffer_, 0, viewports_vk.size(), viewports_vk.data());
}

void RenderCommandEncoderVulkan::SetScissors(Span<Scissor> scissors) {
    Vec<VkRect2D> scissors_vk(scissors.Size());
    for (size_t i = 0; i < scissors_vk.size(); i++) {
        scissors_vk[i] = VkRect2D {
            .offset = { scissors[i].x, scissors[i].y },
            .extent = { scissors[i].width, scissors[i].height },
        };
    }
    vkCmdSetScissor(cmd_buffer_, 0, scissors_vk.size(), scissors_vk.data());
}

void RenderCommandEncoderVulkan::BindVertexBuffer(Span<BufferRange> buffers, uint32_t first_binding) {
    BI_ASSERT_MSG(curr_pipeline_, "Call RenderCommandEncoder::BindVertexBuffer() without setting pipeline");

    Vec<VkBuffer> buffers_vk(buffers.size());
    Vec<uint64_t> offsets_vk(buffers.size());
    for (size_t i = 0; i < buffers.size(); i++) {
        buffers_vk[i] = buffers[i].buffer.CastTo<BufferVulkan>()->Raw();
        offsets_vk[i] = buffers[i].offset;
    }
    vkCmdBindVertexBuffers(cmd_buffer_, first_binding, buffers_vk.size(), buffers_vk.data(), offsets_vk.data());
}

void RenderCommandEncoderVulkan::BindIndexBuffer(Ref<Buffer> buffer, uint64_t offset, IndexType index_type) {
    auto buffer_vk = buffer.CastTo<BufferVulkan>()->Raw();
    BI_ASSERT_MSG(curr_pipeline_, "Call RenderCommandEncoder::BindIndexBuffer() without setting pipeline");

    vkCmdBindIndexBuffer(cmd_buffer_, buffer_vk, offset, ToVkIndexType(index_type));
}

void RenderCommandEncoderVulkan::Draw(uint32_t num_vertices, uint32_t num_instance, uint32_t first_vertex,
    uint32_t first_instance) {
    BI_ASSERT_MSG(curr_pipeline_, "Call RenderCommandEncoder::Draw() without setting pipeline");

    vkCmdDraw(cmd_buffer_, num_vertices, num_instance, first_vertex, first_instance);
}

void RenderCommandEncoderVulkan::DrawIndexed(uint32_t num_indices, uint32_t num_instance, uint32_t first_index,
    uint32_t vertex_offset, uint32_t first_instance) {
    BI_ASSERT_MSG(curr_pipeline_, "Call RenderCommandEncoder::DrawIndexed() without setting pipeline");

    vkCmdDrawIndexed(cmd_buffer_, num_indices, num_instance, first_index, vertex_offset, first_instance);
}


ComputeCommandEncoderVulkan::ComputeCommandEncoderVulkan(Ref<DeviceVulkan> device, Ref<CommandEncoderVulkan> base_encoder,
    const std::string &label) : device_(device), base_encoder_(base_encoder), label_(label) {
    cmd_buffer_ = base_encoder->cmd_buffer_;
}

ComputeCommandEncoderVulkan::~ComputeCommandEncoderVulkan() {
    if (!label_.empty()) {
        PopLabel();
    }
    base_encoder_->cmd_buffer_ = cmd_buffer_;
}

Ptr<CommandBuffer> ComputeCommandEncoderVulkan::Finish() {
    vkEndCommandBuffer(cmd_buffer_);
    auto cmd_buffer = Ptr<CommandBufferVulkan>::Make(device_, cmd_buffer_);
    cmd_buffer_ = VK_NULL_HANDLE;
    return cmd_buffer;
}

void ComputeCommandEncoderVulkan::PushLabel(const CommandLabel &label) {
    auto label_vk = ToVkDebugLabel(label);
    vkCmdBeginDebugUtilsLabelEXT(cmd_buffer_, &label_vk);
}

void ComputeCommandEncoderVulkan::PopLabel() {
    vkCmdEndDebugUtilsLabelEXT(cmd_buffer_);
}

void ComputeCommandEncoderVulkan::SetPipeline(Ref<ComputePipeline> pipeline) {
    curr_pipeline_ = pipeline.CastTo<ComputePipelineVulkan>().Get();
    vkCmdBindPipeline(cmd_buffer_, VK_PIPELINE_BIND_POINT_COMPUTE, curr_pipeline_->RawPipeline());

    // descriptor_sets_.resize(curr_pipeline_->ShaderInfo().bindings.bindings.size(), nullptr);
    // for (size_t set = 0; set < descriptor_sets_.size(); set++) {
    //     descriptor_sets_[set] = base_encoder_->context_->GetDescriptorSet(curr_pipeline_, set);
    //     raw_descriptor_sets_[set] = descriptor_sets_[set]->Raw();
    // }
}

void ComputeCommandEncoderVulkan::BindShaderParams(uint32_t set_index, const ShaderParams &values) {
    BI_ASSERT_MSG(curr_pipeline_, "Call ComputeCommandEncoder::BindShaderParams() without setting pipeline");

    VkDescriptorSet descriptor_set = base_encoder_->context_->GetDescriptorSet(curr_pipeline_->RawSetLayout(set_index),
        curr_pipeline_->Desc().layout.sets_layout[set_index], values);

    vkCmdBindDescriptorSets(cmd_buffer_, VK_PIPELINE_BIND_POINT_COMPUTE, curr_pipeline_->RawPipelineLayout(), 0,
        1, &descriptor_set, 0, nullptr);
}

void ComputeCommandEncoderVulkan::PushConstants(const void *data, uint32_t size, uint32_t offset) {
    BI_ASSERT_MSG(curr_pipeline_, "Call ComputeCommandEncoder::PushConstants() without setting pipeline");

    vkCmdPushConstants(cmd_buffer_, curr_pipeline_->RawPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT,
        offset, size, data);
}

void ComputeCommandEncoderVulkan::Dispatch(uint32_t size_x, uint32_t size_y, uint32_t size_z) {
    BI_ASSERT_MSG(curr_pipeline_, "Call ComputeCommandEncoder::Dispatch() without setting pipeline");

    uint32_t x = (size_x + curr_pipeline_->LocalSizeX() - 1) / curr_pipeline_->LocalSizeX();
    uint32_t y = (size_y + curr_pipeline_->LocalSizeY() - 1) / curr_pipeline_->LocalSizeY();
    uint32_t z = (size_z + curr_pipeline_->LocalSizeZ() - 1) / curr_pipeline_->LocalSizeZ();
    vkCmdDispatch(cmd_buffer_, x, y, z);
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
