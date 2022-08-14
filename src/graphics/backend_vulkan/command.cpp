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

Vec<VkBufferCopy> ToVkBufferCopy(Span<BufferCopyDesc> regions) {
    Vec<VkBufferCopy> regions_vk(regions.size());
    for (size_t i = 0; i < regions.size(); i++) {
        regions_vk[i] = VkBufferCopy {
            .srcOffset = regions[i].src_offset,
            .dstOffset = regions[i].dst_offset,
            .size = regions[i].length,
        };
    }
    return regions_vk;
}

Vec<VkImageCopy> ToVkImageCopy(TextureVulkan *src_texture_vk, TextureVulkan *dst_texture_vk,
    Span<TextureCopyDesc> regions) {
    Vec<VkImageCopy> regions_vk(regions.size());
    for (size_t i = 0; i < regions.size(); i++) {
        uint32_t src_base_depth, src_base_layer;
        src_texture_vk->GetDepthAndLayer(regions[i].src_offset.z, src_base_depth, src_base_layer);
        uint32_t dst_base_depth, dst_base_layer;
        dst_texture_vk->GetDepthAndLayer(regions[i].dst_offset.z, dst_base_depth, dst_base_layer);
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

Vec<VkBufferImageCopy> ToVkBufferImageCopy(TextureVulkan *texture_vk, Span<BufferTextureCopyDesc> regions) {
    Vec<VkBufferImageCopy> regions_vk(regions.size());
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
    auto src_buffer_vk = static_cast<BufferVulkan *>(src_buffer.Get());
    auto dst_buffer_vk = static_cast<BufferVulkan *>(dst_buffer.Get());
    auto regions_vk = ToVkBufferCopy(regions);
    vkCmdCopyBuffer(cmd_buffer_, src_buffer_vk->Raw(), dst_buffer_vk->Raw(), regions_vk.size(), regions_vk.data());
}

void CommandEncoderVulkan::CopyTextureToTexture(Ref<Texture> src_texture, Ref<Texture> dst_texture,
    Span<TextureCopyDesc> regions) {
    auto src_texture_vk = static_cast<TextureVulkan *>(src_texture.Get());
    auto dst_texture_vk = static_cast<TextureVulkan *>(dst_texture.Get());
    auto regions_vk = ToVkImageCopy(src_texture_vk, dst_texture_vk, regions);
    vkCmdCopyImage(cmd_buffer_, src_texture_vk->Raw(), src_texture_vk->Layout(), dst_texture_vk->Raw(),
        dst_texture_vk->Layout(), regions_vk.size(), regions_vk.data());
}

void CommandEncoderVulkan::CopyBufferToTexture(Ref<Buffer> src_buffer, Ref<Texture> dst_texture,
    Span<BufferTextureCopyDesc> regions) {
    auto src_buffer_vk = static_cast<BufferVulkan *>(src_buffer.Get());
    auto dst_texture_vk = static_cast<TextureVulkan *>(dst_texture.Get());
    auto regions_vk = ToVkBufferImageCopy(dst_texture_vk, regions);
    vkCmdCopyBufferToImage(cmd_buffer_, src_buffer_vk->Raw(), dst_texture_vk->Raw(), dst_texture_vk->Layout(),
        regions_vk.size(), regions_vk.data());
}

void CommandEncoderVulkan::CopyTextureToBuffer(Ref<Texture> src_texture, Ref<Buffer> dst_buffer,
    Span<BufferTextureCopyDesc> regions) {
    auto src_texture_vk = static_cast<TextureVulkan *>(src_texture.Get());
    auto dst_buffer_vk = static_cast<BufferVulkan *>(dst_buffer.Get());
    auto regions_vk = ToVkBufferImageCopy(src_texture_vk, regions);
    vkCmdCopyImageToBuffer(cmd_buffer_, src_texture_vk->Raw(), src_texture_vk->Layout(), dst_buffer_vk->Raw(),
        regions_vk.size(), regions_vk.data());
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
                .base_layer = desc.colors[i].texture.base_layer,
                .layers = desc.colors[i].texture.layers,
                .base_level = desc.colors[i].texture.base_level,
                .levels = desc.colors[i].texture.levels,
            }),
            .imageLayout = texture_vk->Layout(),
            .resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT_KHR,
            // TODO - support resolve
            .resolveImageView = nullptr,
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
                .base_layer = depth_stencil.texture.base_layer,
                .layers = depth_stencil.texture.layers,
                .base_level = depth_stencil.texture.base_level,
                .levels = depth_stencil.texture.levels,
            }),
            .imageLayout = texture_vk->Layout(),
            .resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT_KHR,
            .resolveImageView = nullptr,
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
    vkCmdBeginRenderingKHR(cmd_buffer_, &rendering_info);
}

RenderCommandEncoderVulkan::~RenderCommandEncoderVulkan() {
    vkCmdEndRenderingKHR(cmd_buffer_);

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

    descriptor_sets_.resize(curr_pipeline_->ShaderInfo().bindings.bindings.size(), nullptr);
    for (size_t set = 0; set < descriptor_sets_.size(); set++) {
        descriptor_sets_[set] = base_encoder_->context_->GetDescriptorSet(curr_pipeline_, set);
        raw_descriptor_sets_[set] = descriptor_sets_[set]->Raw();
    }
}

void RenderCommandEncoderVulkan::BindBuffer(const std::string &name, const BufferRange &buffer) {
    BI_ASSERT_MSG(curr_pipeline_, "Call RenderCommandEncoder::BindBuffer() without setting pipeline");
    
    const auto &name_map = curr_pipeline_->ShaderInfo().bindings.name_map;
    if (auto it = name_map.find(name); it != name_map.end()) {
        descriptor_sets_[it->second.first]->BindBuffer(it->second.second, buffer);
    }
}

void RenderCommandEncoderVulkan::BindTexture(const std::string &name, const TextureRange &texture) {
    BI_ASSERT_MSG(curr_pipeline_, "Call RenderCommandEncoder::BindTexture() without setting pipeline");
    
    const auto &name_map = curr_pipeline_->ShaderInfo().bindings.name_map;
    if (auto it = name_map.find(name); it != name_map.end()) {
        descriptor_sets_[it->second.first]->BindTexture(it->second.second, texture);
    }
}

void RenderCommandEncoderVulkan::BindSampler(const std::string &name, Ref<Sampler> sampler) {
    BI_ASSERT_MSG(curr_pipeline_, "Call RenderCommandEncoder::BindSampler() without setting pipeline");
    
    const auto &name_map = curr_pipeline_->ShaderInfo().bindings.name_map;
    if (auto it = name_map.find(name); it != name_map.end()) {
        descriptor_sets_[it->second.first]->BindSampler(it->second.second, sampler);
    }
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

    for (DescriptorSetVulkan *set : descriptor_sets_) {
        set->Update();
    }
    vkCmdBindDescriptorSets(cmd_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, curr_pipeline_->RawPipelineLayout(), 0,
        raw_descriptor_sets_.size(), raw_descriptor_sets_.data(), 0, nullptr);

    vkCmdDraw(cmd_buffer_, num_vertices, num_instance, first_vertex, first_instance);
}

void RenderCommandEncoderVulkan::DrawIndexed(uint32_t num_indices, uint32_t num_instance, uint32_t first_index,
    uint32_t vertex_offset, uint32_t first_instance) {
    BI_ASSERT_MSG(curr_pipeline_, "Call RenderCommandEncoder::DrawIndexed() without setting pipeline");

    for (DescriptorSetVulkan *set : descriptor_sets_) {
        set->Update();
    }
    vkCmdBindDescriptorSets(cmd_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, curr_pipeline_->RawPipelineLayout(), 0,
        raw_descriptor_sets_.size(), raw_descriptor_sets_.data(), 0, nullptr);

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

    descriptor_sets_.resize(curr_pipeline_->ShaderInfo().bindings.bindings.size(), nullptr);
    for (size_t set = 0; set < descriptor_sets_.size(); set++) {
        descriptor_sets_[set] = base_encoder_->context_->GetDescriptorSet(curr_pipeline_, set);
        raw_descriptor_sets_[set] = descriptor_sets_[set]->Raw();
    }
}

void ComputeCommandEncoderVulkan::BindBuffer(const std::string &name, const BufferRange &buffer) {
    BI_ASSERT_MSG(curr_pipeline_, "Call ComputeCommandEncoder::BindBuffer() without setting pipeline");
    
    const auto &name_map = curr_pipeline_->ShaderInfo().bindings.name_map;
    if (auto it = name_map.find(name); it != name_map.end()) {
        descriptor_sets_[it->second.first]->BindBuffer(it->second.second, buffer);
    }
}

void ComputeCommandEncoderVulkan::BindTexture(const std::string &name, const TextureRange &texture) {
    BI_ASSERT_MSG(curr_pipeline_, "Call ComputeCommandEncoder::BindTexture() without setting pipeline");
    
    const auto &name_map = curr_pipeline_->ShaderInfo().bindings.name_map;
    if (auto it = name_map.find(name); it != name_map.end()) {
        descriptor_sets_[it->second.first]->BindTexture(it->second.second, texture);
    }
}

void ComputeCommandEncoderVulkan::BindSampler(const std::string &name, Ref<Sampler> sampler) {
    BI_ASSERT_MSG(curr_pipeline_, "Call ComputeCommandEncoder::BindSampler() without setting pipeline");
    
    const auto &name_map = curr_pipeline_->ShaderInfo().bindings.name_map;
    if (auto it = name_map.find(name); it != name_map.end()) {
        descriptor_sets_[it->second.first]->BindSampler(it->second.second, sampler);
    }
}

void ComputeCommandEncoderVulkan::Dispatch(uint32_t size_x, uint32_t size_y, uint32_t size_z) {
    BI_ASSERT_MSG(curr_pipeline_, "Call ComputeCommandEncoder::Dispatch() without setting pipeline");

    for (DescriptorSetVulkan *set : descriptor_sets_) {
        set->Update();
    }
    vkCmdBindDescriptorSets(cmd_buffer_, VK_PIPELINE_BIND_POINT_COMPUTE, curr_pipeline_->RawPipelineLayout(), 0,
        raw_descriptor_sets_.size(), raw_descriptor_sets_.data(), 0, nullptr);

    uint32_t x = (size_x + curr_pipeline_->LocalSizeX() - 1) / curr_pipeline_->LocalSizeX();
    uint32_t y = (size_y + curr_pipeline_->LocalSizeY() - 1) / curr_pipeline_->LocalSizeY();
    uint32_t z = (size_z + curr_pipeline_->LocalSizeZ() - 1) / curr_pipeline_->LocalSizeZ();
    vkCmdDispatch(cmd_buffer_, x, y, z);
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
