#include "command.hpp"

#include "core/logger.hpp"

#include "utils.hpp"
#include "device.hpp"
#include "resource.hpp"

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

std::vector<VkBufferCopy> ToVkBufferCopy(Span<BufferCopyDesc> regions) {
    std::vector<VkBufferCopy> regions_vk(regions.size());
    for (size_t i = 0; i < regions.size(); i++) {
        regions_vk[i] = VkBufferCopy {
            .srcOffset = regions[i].src_offset,
            .dstOffset = regions[i].dst_offset,
            .size = regions[i].length,
        };
    }
    return regions_vk;
}

std::vector<VkImageCopy> ToVkImageCopy(TextureVulkan *src_texture_vk, TextureVulkan *dst_texture_vk,
    Span<TextureCopyDesc> regions) {
    std::vector<VkImageCopy> regions_vk(regions.size());
    for (size_t i = 0; i < regions.size(); i++) {
        uint32_t src_base_depth, src_base_layer;
        src_texture_vk->GetDepthAndLayer(regions[i].src_offset.z, src_base_depth, src_base_layer);
        uint32_t dst_base_depth, dst_base_layer;
        src_texture_vk->GetDepthAndLayer(regions[i].dst_offset.z, dst_base_depth, dst_base_layer);
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

std::vector<VkBufferImageCopy> ToVkBufferImageCopy(TextureVulkan *texture_vk, Span<BufferTextureCopyDesc> regions) {
    std::vector<VkBufferImageCopy> regions_vk(regions.size());
    for (size_t i = 0; i < regions.size(); i++) {
        uint32_t base_depth, base_layer;
        texture_vk->GetDepthAndLayer(regions[i].texture_offset.z, base_depth, base_layer);
        uint32_t depth, layers;
        texture_vk->GetDepthAndLayer(regions[i].texture_extent.depth_or_layers, depth, layers);
        regions_vk[i] = VkBufferImageCopy {
            .bufferOffset = regions[i].buffer_offset,
            .bufferRowLength = regions[i].buffer_bytes_per_row,
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

}

CommandBufferVulkan::CommandBufferVulkan(DeviceVulkan *device, VkCommandBuffer cmd_buffer)
    : device_(device), cmd_buffer_(cmd_buffer) {}

CommandEncoderVulkan::CommandEncoderVulkan(DeviceVulkan *device, VkCommandBuffer cmd_buffer)
    : device_(device), cmd_buffer_(cmd_buffer) {}

CommandEncoderVulkan::~CommandEncoderVulkan() {
    BI_ASSERT(cmd_buffer_ == VK_NULL_HANDLE);
}

Ptr<CommandBuffer> CommandEncoderVulkan::Finish() {
    vkEndCommandBuffer(cmd_buffer_);
    auto cmd_buffer = std::make_unique<CommandBufferVulkan>(device_, cmd_buffer_);
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

void CommandEncoderVulkan::CopyBufferToBuffer(Buffer *src_buffer, Buffer *dst_buffer,
    Span<BufferCopyDesc> regions) {
    auto src_buffer_vk = static_cast<BufferVulkan *>(src_buffer);
    auto dst_buffer_vk = static_cast<BufferVulkan *>(dst_buffer);
    auto regions_vk = ToVkBufferCopy(regions);
    vkCmdCopyBuffer(cmd_buffer_, src_buffer_vk->Raw(), dst_buffer_vk->Raw(), regions_vk.size(), regions_vk.data());
}

void CommandEncoderVulkan::CopyTextureToTexture(Texture *src_texture, Texture *dst_texture,
    Span<TextureCopyDesc> regions) {
    auto src_texture_vk = static_cast<TextureVulkan *>(src_texture);
    auto dst_texture_vk = static_cast<TextureVulkan *>(dst_texture);
    auto regions_vk = ToVkImageCopy(src_texture_vk, dst_texture_vk, regions);
    vkCmdCopyImage(cmd_buffer_, src_texture_vk->Raw(), src_texture_vk->Layout(), dst_texture_vk->Raw(),
        dst_texture_vk->Layout(), regions_vk.size(), regions_vk.data());
}

void CommandEncoderVulkan::CopyBufferToTexture(Buffer *src_buffer, Texture *dst_texture,
    Span<BufferTextureCopyDesc> regions) {
    auto src_buffer_vk = static_cast<BufferVulkan *>(src_buffer);
    auto dst_texture_vk = static_cast<TextureVulkan *>(dst_texture);
    auto regions_vk = ToVkBufferImageCopy(dst_texture_vk, regions);
    vkCmdCopyBufferToImage(cmd_buffer_, src_buffer_vk->Raw(), dst_texture_vk->Raw(), dst_texture_vk->Layout(),
        regions_vk.size(), regions_vk.data());
}

void CommandEncoderVulkan::CopyTextureToBuffer(Texture *src_texture, Buffer *dst_buffer,
    Span<BufferTextureCopyDesc> regions) {
    auto src_texture_vk = static_cast<TextureVulkan *>(src_texture);
    auto dst_buffer_vk = static_cast<BufferVulkan *>(dst_buffer);
    auto regions_vk = ToVkBufferImageCopy(src_texture_vk, regions);
    vkCmdCopyImageToBuffer(cmd_buffer_, src_texture_vk->Raw(), src_texture_vk->Layout(), dst_buffer_vk->Raw(),
        regions_vk.size(), regions_vk.data());
}

Ptr<RenderCommandEncoder> CommandEncoderVulkan::BeginRenderPass(const CommandLabel &label,
    const RenderTargetDesc &desc) {
    if (!label.label.empty()) {
        PushLabel(label);
    }
    
    auto render_encoder = std::make_unique<RenderCommandEncoderVulkan>(device_, desc, this, label.label);
    cmd_buffer_ = VK_NULL_HANDLE;
    return render_encoder;
}

Ptr<ComputeCommandEncoder> CommandEncoderVulkan::BeginComputePass(const CommandLabel &label) {
    if (!label.label.empty()) {
        PushLabel(label);
    }
    auto compute_encoder = std::make_unique<ComputeCommandEncoderVulkan>(device_, this, label.label);
    cmd_buffer_ = VK_NULL_HANDLE;
    return compute_encoder;
}


RenderCommandEncoderVulkan::RenderCommandEncoderVulkan(DeviceVulkan *device, const RenderTargetDesc &desc,
    CommandEncoderVulkan *base_encoder, const std::string &label) {
    device_ = device;
    base_encoder_ = base_encoder;
    label_ = label;
    cmd_buffer_ = base_encoder->cmd_buffer_;

    std::vector<const TextureVulkan *> color_textures_vk;
    color_textures_vk.reserve(kMaxRenderTargetsCount);
    for (uint32_t i = 0; i < kMaxRenderTargetsCount; i++) {
        if (desc.colors[i].texture.texture == nullptr) {
            break;
        }

        color_textures_vk.push_back(static_cast<const TextureVulkan *>(desc.colors[i].texture.texture));
    }

    std::vector<VkAttachmentDescription> attachments_vk;
    attachments_vk.reserve(kMaxRenderTargetsCount + 1);
    for (uint32_t i = 0; i < kMaxRenderTargetsCount; i++) {
        if (desc.colors[i].texture.texture == nullptr) {
            break;
        }

        VkAttachmentDescription attachment {
            .flags = 0,
            .format = color_textures_vk[i]->Format(),
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = desc.colors[i].clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = desc.colors[i].store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = color_textures_vk[i]->Layout(),
            .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
        attachments_vk.push_back(attachment);
    }
    std::vector<VkAttachmentReference> color_attachments_ref(attachments_vk.size());
    for (size_t i = 0; i < attachments_vk.size(); i++) {
        color_attachments_ref[i] = VkAttachmentReference {
            .attachment = static_cast<uint32_t>(i),
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
    }

    const TextureVulkan *depth_stencil_texture_vk = nullptr;
    bool has_depth_stencil_target = desc.depth_stencil.texture.texture;
    VkAttachmentReference depth_stencil_attachment_ref;
    if (has_depth_stencil_target) {
        depth_stencil_texture_vk = static_cast<const TextureVulkan *>(desc.depth_stencil.texture.texture);
        bool has_stencil = IsDepthStencilFormat(depth_stencil_texture_vk->Desc().format);
        VkAttachmentDescription attachment {
            .flags = 0,
            .format = depth_stencil_texture_vk->Format(),
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = desc.depth_stencil.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = desc.depth_stencil.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp = (has_stencil && desc.depth_stencil.clear)
                ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
            .stencilStoreOp = (has_stencil && desc.depth_stencil.store)
                ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = depth_stencil_texture_vk->Layout(),
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };
        attachments_vk.push_back(attachment);

        depth_stencil_attachment_ref = VkAttachmentReference {
            .attachment = static_cast<uint32_t>(attachments_vk.size()) - 1,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };
    }

    BI_ASSERT(color_textures_vk.size() > 0 || depth_stencil_texture_vk != nullptr);

    VkSubpassDescription subpass {
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = static_cast<uint32_t>(color_attachments_ref.size()),
        .pColorAttachments = color_attachments_ref.data(),
        .pResolveAttachments = nullptr,
        .pDepthStencilAttachment = has_depth_stencil_target ? &depth_stencil_attachment_ref : nullptr,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = nullptr,
    };
    std::array<VkSubpassDependency, 2> subpass_dependencies = {
        VkSubpassDependency {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        },
        VkSubpassDependency {
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        },
    };

    VkRenderPassCreateInfo render_pass_ci {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .attachmentCount = static_cast<uint32_t>(attachments_vk.size()),
        .pAttachments = attachments_vk.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 2,
        .pDependencies = subpass_dependencies.data(),
    };
    // TODO - cache render pass
    vkCreateRenderPass(device_->Raw(), &render_pass_ci, nullptr, &render_pass_);

    std::vector<VkImageView> image_views(attachments_vk.size());
    for (size_t i = 0; i < color_attachments_ref.size(); i++) {
        image_views[i] = color_textures_vk[i]->GetView(TextureViewVulkanDesc {
            .base_layer = desc.colors[i].texture.base_layer,
            .layers = desc.colors[i].texture.layers,
            .base_level = desc.colors[i].texture.base_level,
            .levels = desc.colors[i].texture.levels,
        });
    }
    if (has_depth_stencil_target) {
        image_views[color_attachments_ref.size()] = depth_stencil_texture_vk->GetView(TextureViewVulkanDesc {
            .base_layer = desc.depth_stencil.texture.base_layer,
            .layers = desc.depth_stencil.texture.layers,
            .base_level = desc.depth_stencil.texture.base_level,
            .levels = desc.depth_stencil.texture.levels,
        });
    }

    const auto extent = has_depth_stencil_target
        ? depth_stencil_texture_vk->Desc().extent : color_textures_vk[0]->Desc().extent;

    VkFramebufferCreateInfo frame_buffer_ci {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .renderPass = render_pass_,
        .attachmentCount = static_cast<uint32_t>(image_views.size()),
        .pAttachments = image_views.data(),
        .width = extent.width,
        .height = extent.height,
        .layers = 1,
    };
    // TODO - cache frame buffer
    vkCreateFramebuffer(device_->Raw(), &frame_buffer_ci, nullptr, &frame_buffer_);

    std::vector<VkClearValue> clear_values(attachments_vk.size());
    for (size_t i = 0; i < color_attachments_ref.size(); i++) {
        const auto &clear_color = desc.colors[i].clear_color;
        clear_values[i] = VkClearValue {
            .color = VkClearColorValue {
                .float32 = { clear_color.r, clear_color.g, clear_color.b, clear_color.a },
            },
        };
    }
    if (has_depth_stencil_target) {
        clear_values[color_attachments_ref.size()] = VkClearValue {
            .depthStencil = VkClearDepthStencilValue {
                .depth = desc.depth_stencil.clear_depth,
                .stencil = desc.depth_stencil.clear_stencil,
            },
        };
    }
    VkRenderPassBeginInfo begin_info {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = render_pass_,
        .framebuffer = frame_buffer_,
        .renderArea = { { 0, 0 }, { extent.width, extent.height } },
        .clearValueCount = static_cast<uint32_t>(clear_values.size()),
        .pClearValues = clear_values.data(),
    };
    vkCmdBeginRenderPass(cmd_buffer_, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

RenderCommandEncoderVulkan::~RenderCommandEncoderVulkan() {
    vkCmdEndRenderPass(cmd_buffer_);

    if (!label_.empty()) {
        PopLabel();
    }
    if (base_encoder_) {
        base_encoder_->cmd_buffer_ = cmd_buffer_;
    } else {
        BI_ASSERT(cmd_buffer_ == VK_NULL_HANDLE);
    }
}

Ptr<CommandBuffer> RenderCommandEncoderVulkan::Finish() {
    vkEndCommandBuffer(cmd_buffer_);
    auto cmd_buffer = std::make_unique<CommandBufferVulkan>(device_, cmd_buffer_);
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

void RenderCommandEncoderVulkan::CopyBufferToBuffer(Buffer *src_buffer, Buffer *dst_buffer,
    Span<BufferCopyDesc> regions) {
    auto src_buffer_vk = static_cast<BufferVulkan *>(src_buffer);
    auto dst_buffer_vk = static_cast<BufferVulkan *>(dst_buffer);
    auto regions_vk = ToVkBufferCopy(regions);
    vkCmdCopyBuffer(cmd_buffer_, src_buffer_vk->Raw(), dst_buffer_vk->Raw(), regions_vk.size(), regions_vk.data());
}

void RenderCommandEncoderVulkan::CopyTextureToTexture(Texture *src_texture, Texture *dst_texture,
    Span<TextureCopyDesc> regions) {
    auto src_texture_vk = static_cast<TextureVulkan *>(src_texture);
    auto dst_texture_vk = static_cast<TextureVulkan *>(dst_texture);
    auto regions_vk = ToVkImageCopy(src_texture_vk, dst_texture_vk, regions);
    vkCmdCopyImage(cmd_buffer_, src_texture_vk->Raw(), src_texture_vk->Layout(), dst_texture_vk->Raw(),
        dst_texture_vk->Layout(), regions_vk.size(), regions_vk.data());
}

void RenderCommandEncoderVulkan::CopyBufferToTexture(Buffer *src_buffer, Texture *dst_texture,
    Span<BufferTextureCopyDesc> regions) {
    auto src_buffer_vk = static_cast<BufferVulkan *>(src_buffer);
    auto dst_texture_vk = static_cast<TextureVulkan *>(dst_texture);
    auto regions_vk = ToVkBufferImageCopy(dst_texture_vk, regions);
    vkCmdCopyBufferToImage(cmd_buffer_, src_buffer_vk->Raw(), dst_texture_vk->Raw(), dst_texture_vk->Layout(),
        regions_vk.size(), regions_vk.data());
}

void RenderCommandEncoderVulkan::CopyTextureToBuffer(Texture *src_texture, Buffer *dst_buffer,
    Span<BufferTextureCopyDesc> regions) {
    auto src_texture_vk = static_cast<TextureVulkan *>(src_texture);
    auto dst_buffer_vk = static_cast<BufferVulkan *>(dst_buffer);
    auto regions_vk = ToVkBufferImageCopy(src_texture_vk, regions);
    vkCmdCopyImageToBuffer(cmd_buffer_, src_texture_vk->Raw(), src_texture_vk->Layout(), dst_buffer_vk->Raw(),
        regions_vk.size(), regions_vk.data());
}

void RenderCommandEncoderVulkan::BindVertexBuffer(Span<std::pair<Buffer *, uint64_t>> buffers,
    uint32_t first_binding) {
    std::vector<VkBuffer> buffers_vk(buffers.size());
    std::vector<uint64_t> offsets_vk(buffers.size());
    for (size_t i = 0; i < buffers.size(); i++) {
        buffers_vk[i] = static_cast<BufferVulkan *>(buffers[i].first)->Raw();
        offsets_vk[i] = buffers[i].second;
    }
    vkCmdBindVertexBuffers(cmd_buffer_, first_binding, buffers_vk.size(), buffers_vk.data(), offsets_vk.data());
}

void RenderCommandEncoderVulkan::BindIndexBuffer(Buffer *buffer, uint64_t offset, IndexType index_type) {
    auto buffer_vk = static_cast<BufferVulkan *>(buffer)->Raw();
    vkCmdBindIndexBuffer(cmd_buffer_, buffer_vk, offset, ToVkIndexType(index_type));
}

void RenderCommandEncoderVulkan::Draw(uint32_t num_vertices, uint32_t num_instance, uint32_t first_vertex,
    uint32_t first_instance) {
    vkCmdDraw(cmd_buffer_, num_vertices, num_instance, first_vertex, first_instance);
}

void RenderCommandEncoderVulkan::DrawIndexed(uint32_t num_indices, uint32_t num_instance, uint32_t first_index,
    uint32_t vertex_offset, uint32_t first_instance) {
    vkCmdDrawIndexed(cmd_buffer_, num_indices, num_instance, first_index, vertex_offset, first_instance);
}


ComputeCommandEncoderVulkan::ComputeCommandEncoderVulkan(DeviceVulkan *device, CommandEncoderVulkan *base_encoder,
    const std::string &label) {
    device_ = device;
    base_encoder_ = base_encoder;
    label_ = label;
    cmd_buffer_ = base_encoder->cmd_buffer_;
}

ComputeCommandEncoderVulkan::~ComputeCommandEncoderVulkan() {
    if (!label_.empty()) {
        PopLabel();
    }
    if (base_encoder_) {
        base_encoder_->cmd_buffer_ = cmd_buffer_;
    } else {
        BI_ASSERT(cmd_buffer_ == VK_NULL_HANDLE);
    }
}

Ptr<CommandBuffer> ComputeCommandEncoderVulkan::Finish() {
    vkEndCommandBuffer(cmd_buffer_);
    auto cmd_buffer = std::make_unique<CommandBufferVulkan>(device_, cmd_buffer_);
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

void ComputeCommandEncoderVulkan::CopyBufferToBuffer(Buffer *src_buffer, Buffer *dst_buffer,
    Span<BufferCopyDesc> regions) {
    auto src_buffer_vk = static_cast<BufferVulkan *>(src_buffer);
    auto dst_buffer_vk = static_cast<BufferVulkan *>(dst_buffer);
    auto regions_vk = ToVkBufferCopy(regions);
    vkCmdCopyBuffer(cmd_buffer_, src_buffer_vk->Raw(), dst_buffer_vk->Raw(), regions_vk.size(), regions_vk.data());
}

void ComputeCommandEncoderVulkan::CopyTextureToTexture(Texture *src_texture, Texture *dst_texture,
    Span<TextureCopyDesc> regions) {
    auto src_texture_vk = static_cast<TextureVulkan *>(src_texture);
    auto dst_texture_vk = static_cast<TextureVulkan *>(dst_texture);
    auto regions_vk = ToVkImageCopy(src_texture_vk, dst_texture_vk, regions);
    vkCmdCopyImage(cmd_buffer_, src_texture_vk->Raw(), src_texture_vk->Layout(), dst_texture_vk->Raw(),
        dst_texture_vk->Layout(), regions_vk.size(), regions_vk.data());
}

void ComputeCommandEncoderVulkan::CopyBufferToTexture(Buffer *src_buffer, Texture *dst_texture,
    Span<BufferTextureCopyDesc> regions) {
    auto src_buffer_vk = static_cast<BufferVulkan *>(src_buffer);
    auto dst_texture_vk = static_cast<TextureVulkan *>(dst_texture);
    auto regions_vk = ToVkBufferImageCopy(dst_texture_vk, regions);
    vkCmdCopyBufferToImage(cmd_buffer_, src_buffer_vk->Raw(), dst_texture_vk->Raw(), dst_texture_vk->Layout(),
        regions_vk.size(), regions_vk.data());
}

void ComputeCommandEncoderVulkan::CopyTextureToBuffer(Texture *src_texture, Buffer *dst_buffer,
    Span<BufferTextureCopyDesc> regions) {
    auto src_texture_vk = static_cast<TextureVulkan *>(src_texture);
    auto dst_buffer_vk = static_cast<BufferVulkan *>(dst_buffer);
    auto regions_vk = ToVkBufferImageCopy(src_texture_vk, regions);
    vkCmdCopyImageToBuffer(cmd_buffer_, src_texture_vk->Raw(), src_texture_vk->Layout(), dst_buffer_vk->Raw(),
        regions_vk.size(), regions_vk.data());
}

void ComputeCommandEncoderVulkan::Dispatch(uint32_t x, uint32_t y, uint32_t z) {
    vkCmdDispatch(cmd_buffer_, x, y, z);
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
