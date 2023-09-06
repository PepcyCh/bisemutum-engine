#include "command.hpp"

#include "volk.h"
#include "utils.hpp"
#include "device.hpp"
#include "descriptor.hpp"
#include "resource.hpp"
#include "pipeline.hpp"
#include "queue.hpp"

namespace bi::rhi {

namespace {

auto push_label_impl(VkCommandBuffer cmd_buffer, CommandLabel const& label) -> void {
    if (vkCmdBeginDebugUtilsLabelEXT) {
        auto owned_string = std::string{label.label};
        VkDebugUtilsLabelEXT label_vk{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
            .pNext = nullptr,
            .pLabelName = owned_string.c_str(),
            .color = {label.color.r, label.color.g, label.color.b, 1.0f},
        };
        vkCmdBeginDebugUtilsLabelEXT(cmd_buffer, &label_vk);
    }
}

auto pop_label_impl(VkCommandBuffer cmd_buffer) -> void {
    if (vkCmdEndDebugUtilsLabelEXT) {
        vkCmdEndDebugUtilsLabelEXT(cmd_buffer);
    }
}

auto to_vk_buffer_access_type(
    BitFlags<ResourceAccessType> type, VkAccessFlags2 &type_vk, VkPipelineStageFlags2 &stage_vk
) -> void {
    type_vk = 0;
    stage_vk = 0;
    if (type.contains(ResourceAccessType::vertex_buffer_read)) {
        type_vk |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
        stage_vk |= VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
    }
    if (type.contains(ResourceAccessType::index_buffer_read)) {
        type_vk |= VK_ACCESS_2_INDEX_READ_BIT;
        stage_vk |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
    }
    if (type.contains(ResourceAccessType::indirect_read)) {
        type_vk |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
        stage_vk |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
    }
    if (type.contains(ResourceAccessType::uniform_buffer_read)) {
        type_vk |= VK_ACCESS_2_UNIFORM_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT;
        stage_vk |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
            | VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    }
    if (type.contains(ResourceAccessType::storage_resource_read)) {
        type_vk |= VK_ACCESS_2_SHADER_READ_BIT;
        stage_vk |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
            | VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    }
    if (type.contains(ResourceAccessType::storage_resource_write)) {
        type_vk |= VK_ACCESS_2_SHADER_WRITE_BIT;
        stage_vk |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
            | VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    }
    if (type.contains(ResourceAccessType::transfer_read)) {
        type_vk |= VK_ACCESS_2_TRANSFER_READ_BIT;
        stage_vk |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    }
    if (type.contains(ResourceAccessType::transfer_write)) {
        type_vk |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
        stage_vk |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    }
}

auto to_vk_image_access_type(
    BitFlags<ResourceAccessType> type, bool is_depth_stencil,
    VkAccessFlags2 &type_vk, VkPipelineStageFlags2 &stage_vk, VkImageLayout &layout_vk
) -> void {
    type_vk = 0;
    stage_vk = 0;
    layout_vk = VK_IMAGE_LAYOUT_UNDEFINED;
    if (type.contains(ResourceAccessType::sampled_texture_read)) {
        type_vk |= VK_ACCESS_2_SHADER_READ_BIT;
        stage_vk |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
            | VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
        layout_vk = is_depth_stencil ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
            : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    if (type.contains(ResourceAccessType::storage_resource_read)) {
        type_vk |= VK_ACCESS_2_SHADER_READ_BIT;
        stage_vk |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
            | VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
        layout_vk = VK_IMAGE_LAYOUT_GENERAL;
    }
    if (type.contains(ResourceAccessType::storage_resource_write)) {
        type_vk |= VK_ACCESS_2_SHADER_WRITE_BIT;
        stage_vk |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
            | VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
        layout_vk = VK_IMAGE_LAYOUT_GENERAL;
    }
    if (type.contains(ResourceAccessType::color_attachment_read)) {
        type_vk |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
        stage_vk |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        layout_vk = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    if (type.contains(ResourceAccessType::depth_stencil_attachment_read)) {
        type_vk |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        stage_vk |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        layout_vk = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    }
    if (type.contains(ResourceAccessType::color_attachment_write)) {
        type_vk |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        stage_vk |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        layout_vk = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    if (type.contains(ResourceAccessType::depth_stencil_attachment_write)) {
        type_vk |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        stage_vk |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        layout_vk = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }
    if (type.contains(ResourceAccessType::transfer_read)) {
        type_vk |= VK_ACCESS_2_TRANSFER_READ_BIT;
        stage_vk |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        layout_vk = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }
    if (type.contains(ResourceAccessType::transfer_write)) {
        type_vk |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
        stage_vk |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        layout_vk = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    }
    if (type.contains(ResourceAccessType::resolve_read)) {
        type_vk |= VK_ACCESS_2_TRANSFER_READ_BIT;
        stage_vk |= VK_PIPELINE_STAGE_2_RESOLVE_BIT;
        layout_vk = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }
    if (type.contains(ResourceAccessType::resolve_write)) {
        type_vk |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
        stage_vk |= VK_PIPELINE_STAGE_2_RESOLVE_BIT;
        layout_vk = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    }
    if (type.contains(ResourceAccessType::present)) {
        stage_vk |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        layout_vk = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }
}

} // namespace

CommandPoolVulkan::CommandPoolVulkan(Ref<DeviceVulkan> device, CommandPoolDesc const& desc) : device_(device) {
    VkCommandPoolCreateInfo cmd_pool_ci{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueFamilyIndex = desc.queue.cast_to<const QueueVulkan>()->raw_family_index(),
    };
    vkCreateCommandPool(device_->raw(), &cmd_pool_ci, nullptr, &cmd_pool_);
}

CommandPoolVulkan::~CommandPoolVulkan() {
    if (cmd_pool_) {
        vkDestroyCommandPool(device_->raw(), cmd_pool_, nullptr);
        cmd_pool_ = VK_NULL_HANDLE;
    }
}

auto CommandPoolVulkan::reset() -> void {
    vkResetCommandPool(device_->raw(), cmd_pool_, 0);
    available_cmd_buffer_index_ = 0;
}

auto CommandPoolVulkan::get_command_encoder() -> Box<CommandEncoder> {
    VkCommandBuffer cmd_buffer = VK_NULL_HANDLE;
    if (available_cmd_buffer_index_ < allocated_cmd_buffers_.size()) {
        cmd_buffer = allocated_cmd_buffers_[available_cmd_buffer_index_++];
    } else {
        VkCommandBufferAllocateInfo cmd_buffer_ai{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = cmd_pool_,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        vkAllocateCommandBuffers(device_->raw(), &cmd_buffer_ai, &cmd_buffer);
        allocated_cmd_buffers_.push_back(cmd_buffer);
        ++available_cmd_buffer_index_;
    }

    VkCommandBufferBeginInfo begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };
    vkBeginCommandBuffer(cmd_buffer, &begin_info);

    return Box<CommandEncoderVulkan>::make(device_, cmd_buffer);
}


CommandBufferVulkan::CommandBufferVulkan(Ref<DeviceVulkan> device, VkCommandBuffer cmd_buffer)
    : device_(device), cmd_buffer_(cmd_buffer) {}


CommandEncoderVulkan::CommandEncoderVulkan(Ref<DeviceVulkan> device, VkCommandBuffer cmd_buffer)
    : device_(device), cmd_buffer_(cmd_buffer) {}

CommandEncoderVulkan::~CommandEncoderVulkan() {
    assert(cmd_buffer_ == VK_NULL_HANDLE);
}

auto CommandEncoderVulkan::finish() -> Box<CommandBuffer> {
    vkEndCommandBuffer(cmd_buffer_);
    auto cmd_buffer = Box<CommandBufferVulkan>::make(device_, cmd_buffer_);
    cmd_buffer_ = VK_NULL_HANDLE;
    return cmd_buffer;
}

auto CommandEncoderVulkan::push_label(CommandLabel const& label) -> void {
    push_label_impl(cmd_buffer_, label);
}

auto CommandEncoderVulkan::pop_label() -> void {
    pop_label_impl(cmd_buffer_);
}

auto CommandEncoderVulkan::copy_buffer_to_buffer(
    CRef<Buffer> src_buffer, Ref<Buffer> dst_buffer, BufferCopyDesc const& region
) -> void {
    auto src_buffer_vk = src_buffer.cast_to<const BufferVulkan>();
    auto dst_buffer_vk = dst_buffer.cast_to<BufferVulkan>();
    uint64_t length = std::min({
        region.length,
        src_buffer_vk->desc().size - region.src_offset,
        dst_buffer_vk->desc().size - region.dst_offset
    });
    VkBufferCopy region_vk{
        .srcOffset = region.src_offset,
        .dstOffset = region.dst_offset,
        .size = length,
    };
    vkCmdCopyBuffer(cmd_buffer_, src_buffer_vk->raw(), dst_buffer_vk->raw(), 1, &region_vk);
}

auto CommandEncoderVulkan::copy_texture_to_texture(
    CRef<Texture> src_texture, Ref<Texture> dst_texture, TextureCopyDesc const& region
) -> void {
    auto src_texture_vk = src_texture.cast_to<const TextureVulkan>();
    auto dst_texture_vk = dst_texture.cast_to<TextureVulkan>();
    VkImageCopy region_vk{
        .srcSubresource = VkImageSubresourceLayers{
            .aspectMask = src_texture_vk->get_aspect(),
            .mipLevel = region.src_level,
            .baseArrayLayer = region.src_layer,
            .layerCount = 1,
        },
        .srcOffset = VkOffset3D{
            .x = static_cast<int>(region.src_offset.x),
            .y = static_cast<int>(region.src_offset.y),
            .z = static_cast<int>(region.src_offset.z),
        },
        .dstSubresource = VkImageSubresourceLayers{
            .aspectMask = dst_texture_vk->get_aspect(),
            .mipLevel = region.dst_level,
            .baseArrayLayer = region.dst_layer,
            .layerCount = 1,
        },
        .dstOffset = VkOffset3D{
            .x = static_cast<int>(region.dst_offset.x),
            .y = static_cast<int>(region.dst_offset.y),
            .z = static_cast<int>(region.dst_offset.z),
        },
        .extent = VkExtent3D{
            .width = region.extent.width,
            .height = region.extent.height,
            .depth = region.extent.depth_or_layers,
        }
    };
    vkCmdCopyImage(
        cmd_buffer_,
        src_texture_vk->raw(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dst_texture_vk->raw(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &region_vk
    );
}

auto CommandEncoderVulkan::copy_buffer_to_texture(
    CRef<Buffer> src_buffer, Ref<Texture> dst_texture, BufferTextureCopyDesc const& region
) -> void {
    auto src_buffer_vk = src_buffer.cast_to<const BufferVulkan>();
    auto dst_texture_vk = dst_texture.cast_to<TextureVulkan>();
    VkBufferImageCopy region_vk{
        .bufferOffset = region.buffer_offset,
        .bufferRowLength = region.buffer_pixels_per_row,
        .bufferImageHeight = region.buffer_rows_per_texture,
        .imageSubresource = VkImageSubresourceLayers{
            .aspectMask = dst_texture_vk->get_aspect(),
            .mipLevel = region.texture_level,
            .baseArrayLayer = region.texture_layer,
            .layerCount = 1,
        },
        .imageOffset = VkOffset3D{
            .x = static_cast<int>(region.texture_offset.x),
            .y = static_cast<int>(region.texture_offset.y),
            .z = static_cast<int>(region.texture_offset.z),
        },
        .imageExtent = VkExtent3D{
            .width = region.texture_extent.width,
            .height = region.texture_extent.height,
            .depth = region.texture_extent.depth_or_layers,
        },
    };
    vkCmdCopyBufferToImage(
        cmd_buffer_,
        src_buffer_vk->raw(),
        dst_texture_vk->raw(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &region_vk
    );
}

auto CommandEncoderVulkan::copy_texture_to_buffer(
    CRef<Texture> src_texture, Ref<Buffer> dst_buffer, BufferTextureCopyDesc const& region
) -> void {
    auto src_texture_vk = src_texture.cast_to<const TextureVulkan>();
    auto dst_buffer_vk = dst_buffer.cast_to<BufferVulkan>();
    VkBufferImageCopy region_vk{
        .bufferOffset = region.buffer_offset,
        .bufferRowLength = region.buffer_pixels_per_row,
        .bufferImageHeight = region.buffer_rows_per_texture,
        .imageSubresource = VkImageSubresourceLayers{
            .aspectMask = src_texture_vk->get_aspect(),
            .mipLevel = region.texture_level,
            .baseArrayLayer = region.texture_layer,
            .layerCount = 1,
        },
        .imageOffset = VkOffset3D{
            .x = static_cast<int>(region.texture_offset.x),
            .y = static_cast<int>(region.texture_offset.y),
            .z = static_cast<int>(region.texture_offset.z),
        },
        .imageExtent = VkExtent3D{
            .width = region.texture_extent.width,
            .height = region.texture_extent.height,
            .depth = region.texture_extent.depth_or_layers,
        },
    };
    vkCmdCopyImageToBuffer(
        cmd_buffer_,
        src_texture_vk->raw(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dst_buffer_vk->raw(),
        1, &region_vk
    );
}

auto CommandEncoderVulkan::resource_barriers(
    CSpan<BufferBarrier> buffer_barriers, CSpan<TextureBarrier> texture_barriers
) -> void {
    std::vector<VkBufferMemoryBarrier2> buffer_barriers_vk(buffer_barriers.size());
    for (size_t i = 0; i < buffer_barriers.size(); i++) {
        const auto &barrier = buffer_barriers[i];
        buffer_barriers_vk[i] = VkBufferMemoryBarrier2{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcQueueFamilyIndex = barrier.src_queue
                ? barrier.src_queue.value().cast_to<const QueueVulkan>()->raw_family_index()
                : VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = barrier.dst_queue
                ? barrier.dst_queue.value().cast_to<const QueueVulkan>()->raw_family_index()
                : VK_QUEUE_FAMILY_IGNORED,
            .buffer = barrier.buffer.cast_to<BufferVulkan>()->raw(),
            .offset = 0,
            .size = VK_WHOLE_SIZE,
        };
        to_vk_buffer_access_type(
            barrier.src_access_type, buffer_barriers_vk[i].srcAccessMask, buffer_barriers_vk[i].srcStageMask
        );
        to_vk_buffer_access_type(
            barrier.dst_access_type, buffer_barriers_vk[i].dstAccessMask, buffer_barriers_vk[i].dstStageMask
        );
    }
    std::vector<VkImageMemoryBarrier2> texture_barriers_vk(texture_barriers.size());
    for (size_t i = 0; i < texture_barriers.size(); i++) {
        const auto &barrier = texture_barriers[i];
        const auto texture_vk = barrier.texture.cast_to<TextureVulkan>();
        texture_barriers_vk[i] = VkImageMemoryBarrier2{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcQueueFamilyIndex = barrier.src_queue
                ? barrier.src_queue.value().cast_to<const QueueVulkan>()->raw_family_index()
                : VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = barrier.dst_queue
                ? barrier.dst_queue.value().cast_to<const QueueVulkan>()->raw_family_index()
                : VK_QUEUE_FAMILY_IGNORED,
            .image = texture_vk->raw(),
            .subresourceRange = VkImageSubresourceRange{
                .aspectMask = texture_vk->get_aspect(),
                .baseMipLevel = barrier.base_level,
                .levelCount = barrier.num_levels,
                .baseArrayLayer = barrier.base_layer,
                .layerCount = barrier.num_layers,
            },
        };
        bool is_depth_stencil = is_depth_stencil_format(texture_vk->desc().format);
        to_vk_image_access_type(
            barrier.src_access_type, is_depth_stencil, 
            texture_barriers_vk[i].srcAccessMask,
            texture_barriers_vk[i].srcStageMask,
            texture_barriers_vk[i].oldLayout
        );
        to_vk_image_access_type(
            barrier.dst_access_type, is_depth_stencil, 
            texture_barriers_vk[i].dstAccessMask,
            texture_barriers_vk[i].dstStageMask,
            texture_barriers_vk[i].newLayout
        );
    }

    VkDependencyInfo dep_info{
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
    vkCmdPipelineBarrier2(cmd_buffer_, &dep_info);
}

auto CommandEncoderVulkan::set_descriptor_heaps(CSpan<Ref<DescriptorHeap>> heaps) -> void {
    std::vector<VkDescriptorBufferBindingInfoEXT> binding_infos(heaps.size());
    binded_descriptor_heaps_start_.resize(heaps.size());
    for (size_t i = 0; i < heaps.size(); i++) {
        auto heap_vk = heaps[i].cast_to<DescriptorHeapVulkan>();
        binding_infos[i] = VkDescriptorBufferBindingInfoEXT{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
            .pNext = nullptr,
            .address = heap_vk->base_gpu_address(),
            .usage = heap_vk->type(),
        };
        binded_descriptor_heaps_start_[i] = binding_infos[i].address;
    }
    vkCmdBindDescriptorBuffersEXT(cmd_buffer_, binding_infos.size(), binding_infos.data());
}

auto CommandEncoderVulkan::begin_render_pass(
    CommandLabel const& label, RenderTargetDesc const& desc
) -> Box<GraphicsCommandEncoder> {
    if (!label.label.empty()) {
        push_label(label);
    }
    auto render_encoder = Box<GraphicsCommandEncoderVulkan>::make(
        device_, desc, unsafe_make_ref(this), !label.label.empty()
    );
    cmd_buffer_ = VK_NULL_HANDLE;
    return render_encoder;
}

auto CommandEncoderVulkan::begin_compute_pass(CommandLabel const& label) -> Box<ComputeCommandEncoder> {
    if (!label.label.empty()) {
        push_label(label);
    }
    auto compute_encoder = Box<ComputeCommandEncoderVulkan>::make(
        device_, unsafe_make_ref(this), !label.label.empty()
    );
    cmd_buffer_ = VK_NULL_HANDLE;
    return compute_encoder;
}

auto CommandEncoderVulkan::set_descriptors(
    VkCommandBuffer cmd_buffer, VkPipelineBindPoint bind_point, VkPipelineLayout pipline_layout,
    uint32_t from_group_index, CSpan<DescriptorHandle> descriptors
) const -> void {
    std::vector<uint32_t> heap_indices(descriptors.size());
    std::vector<VkDeviceSize> heap_offsets(descriptors.size());
    for (size_t i = 0; i < descriptors.size(); i++) {
        auto const& descriptor = descriptors[i];
        auto& index = heap_indices[i];
        auto& offset = heap_offsets[i];
        for (uint32_t heap_index = 0; auto base_addr : binded_descriptor_heaps_start_) {
            auto temp_offset = descriptor.gpu - base_addr;
            if (heap_index == 0 || temp_offset < offset) {
                index = heap_index;
                offset = temp_offset;
            }
            ++heap_index;
        }
    }
    vkCmdSetDescriptorBufferOffsetsEXT(
        cmd_buffer_, bind_point, pipline_layout, from_group_index,
        descriptors.size(), heap_indices.data(), heap_offsets.data()
    );
}


GraphicsCommandEncoderVulkan::GraphicsCommandEncoderVulkan(
    Ref<DeviceVulkan> device,
    RenderTargetDesc const& desc,
    Ref<CommandEncoderVulkan> base_encoder,
    bool has_label
)
    : device_(device), base_encoder_(base_encoder), has_label_(has_label)
{
    cmd_buffer_ = base_encoder->cmd_buffer_;

    assert(desc.colors.size() > 0 || desc.depth_stencil.has_value());

    Extent3D extent{};

    std::vector<VkRenderingAttachmentInfoKHR> color_attachments_vk(desc.colors.size());
    for (size_t i = 0; i < desc.colors.size(); i++) {
        auto const& target = desc.colors[i];
        auto texture_vk = target.texture.texture.cast_to<TextureVulkan>();
        auto const& clear_color = target.clear_color;
        extent = texture_vk->desc().extent;

        color_attachments_vk[i] = VkRenderingAttachmentInfoKHR{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
            .pNext = nullptr,
            .imageView = texture_vk->get_view(
                target.texture.mip_level, 1, target.texture.base_layer, target.texture.num_layers,
                to_vk_format(texture_vk->desc().format),
                target.texture.num_layers == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY
            ),
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            // TODO - support resolve
            .resolveMode = VK_RESOLVE_MODE_NONE,
            .resolveImageView = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp = desc.colors[i].clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = desc.colors[i].store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .clearValue = VkClearValue{
                .color = VkClearColorValue{
                    .float32 = {clear_color.r, clear_color.g, clear_color.b, clear_color.a},
                },
            }
        };
    }

    auto has_depth_stencil = desc.depth_stencil.has_value();
    auto has_stencil = false;
    VkRenderingAttachmentInfoKHR depth_stencil_attachment_vk{};
    if (has_depth_stencil) {
        auto const& depth_stencil = desc.depth_stencil.value();
        auto texture_vk = depth_stencil.texture.texture.cast_to<TextureVulkan>();
        has_stencil = !is_depth_only_format(texture_vk->desc().format);
        extent = texture_vk->desc().extent;

        depth_stencil_attachment_vk = VkRenderingAttachmentInfoKHR{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
            .pNext = nullptr,
            .imageView = texture_vk->get_view(
                depth_stencil.texture.mip_level, 1, depth_stencil.texture.base_layer, depth_stencil.texture.num_layers,
                to_vk_format(texture_vk->desc().format),
                depth_stencil.texture.num_layers == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY
            ),
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .resolveMode = VK_RESOLVE_MODE_NONE,
            .resolveImageView = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp = depth_stencil.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = depth_stencil.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .clearValue = VkClearValue{
            .depthStencil = VkClearDepthStencilValue{
                    .depth = depth_stencil.clear_depth,
                    .stencil = depth_stencil.clear_stencil,
                },
            },
        };
    }

    VkRenderingInfoKHR rendering_info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .renderArea = {
            .offset = {0, 0},
            .extent = {extent.width, extent.height},
        },
        .layerCount = 1,
        .viewMask = 0,
        .colorAttachmentCount = static_cast<uint32_t>(color_attachments_vk.size()),
        .pColorAttachments = color_attachments_vk.data(),
        .pDepthAttachment = has_depth_stencil ? &depth_stencil_attachment_vk : nullptr,
        .pStencilAttachment = has_stencil ? &depth_stencil_attachment_vk : nullptr,
    };
    vkCmdBeginRendering(cmd_buffer_, &rendering_info);
}

GraphicsCommandEncoderVulkan::~GraphicsCommandEncoderVulkan() {
    vkCmdEndRendering(cmd_buffer_);

    if (has_label_) {
        pop_label();
    }
    base_encoder_->cmd_buffer_ = cmd_buffer_;
}

auto GraphicsCommandEncoderVulkan::push_label(CommandLabel const& label) -> void {
    push_label_impl(cmd_buffer_, label);
}

auto GraphicsCommandEncoderVulkan::pop_label() -> void {
    pop_label_impl(cmd_buffer_);
}

auto GraphicsCommandEncoderVulkan::set_pipeline(CRef<GraphicsPipeline> pipeline) -> void {
    curr_pipeline_ = pipeline.cast_to<const GraphicsPipelineVulkan>().get();
    vkCmdBindPipeline(cmd_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, curr_pipeline_->raw());

    if (auto embedded_samplers_set = curr_pipeline_->static_samplers_set(); embedded_samplers_set != ~0u) {
        vkCmdBindDescriptorBufferEmbeddedSamplersEXT(
            cmd_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, curr_pipeline_->raw_layout(), embedded_samplers_set
        );
    }
}

auto GraphicsCommandEncoderVulkan::set_descriptors(
    uint32_t from_group_index, CSpan<DescriptorHandle> descriptors
) -> void {
    base_encoder_->set_descriptors(
        cmd_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, curr_pipeline_->raw_layout(), from_group_index, descriptors
    );
}

auto GraphicsCommandEncoderVulkan::push_constants(void const* data, uint32_t size, uint32_t offset) -> void {
    vkCmdPushConstants(
        cmd_buffer_, curr_pipeline_->raw_layout(), VK_SHADER_STAGE_ALL_GRAPHICS, offset, size, data
    );
}

auto GraphicsCommandEncoderVulkan::set_viewports(CSpan<Viewport> viewports) -> void {
    std::vector<VkViewport> viewports_vk(viewports.size());
    for (size_t i = 0; i < viewports_vk.size(); i++) {
        // Inverse Vulkan viewport to make result the same as D3D12
        viewports_vk[i] = VkViewport{
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

auto GraphicsCommandEncoderVulkan::set_scissors(CSpan<Scissor> scissors) -> void {
    std::vector<VkRect2D> scissors_vk(scissors.size());
    for (size_t i = 0; i < scissors_vk.size(); i++) {
        scissors_vk[i] = VkRect2D{
            .offset = {scissors[i].x, scissors[i].y},
            .extent = {scissors[i].width, scissors[i].height},
        };
    }
    vkCmdSetScissor(cmd_buffer_, 0, scissors_vk.size(), scissors_vk.data());
}

auto GraphicsCommandEncoderVulkan::set_vertex_buffer(
    CSpan<Ref<Buffer>> buffers, CSpan<uint64_t> offsets, uint32_t first_binding
) -> void {
    std::vector<VkBuffer> buffers_vk(buffers.size());
    for (size_t i = 0; i < buffers.size(); i++) {
        buffers_vk[i] = buffers[i].cast_to<BufferVulkan>()->raw();
    }
    vkCmdBindVertexBuffers(cmd_buffer_, first_binding, buffers_vk.size(), buffers_vk.data(), offsets.data());
}

auto GraphicsCommandEncoderVulkan::set_index_buffer(Ref<Buffer> buffer, uint64_t offset, IndexType index_type) -> void {
    auto buffer_vk = buffer.cast_to<BufferVulkan>()->raw();
    vkCmdBindIndexBuffer(cmd_buffer_, buffer_vk, offset, to_vk_index_type(index_type));
}

auto GraphicsCommandEncoderVulkan::draw(
    uint32_t num_vertices, uint32_t num_instance, uint32_t first_vertex, uint32_t first_instance
) -> void {
    vkCmdDraw(cmd_buffer_, num_vertices, num_instance, first_vertex, first_instance);
}

auto GraphicsCommandEncoderVulkan::draw_indexed(
    uint32_t num_indices, uint32_t num_instance, uint32_t first_index, uint32_t vertex_offset, uint32_t first_instance
) -> void {
    vkCmdDrawIndexed(cmd_buffer_, num_indices, num_instance, first_index, vertex_offset, first_instance);
}


ComputeCommandEncoderVulkan::ComputeCommandEncoderVulkan(
    Ref<DeviceVulkan> device, Ref<CommandEncoderVulkan> base_encoder, bool has_label
)
    : device_(device), base_encoder_(base_encoder), has_label_(has_label)
{
    cmd_buffer_ = base_encoder->cmd_buffer_;
}

ComputeCommandEncoderVulkan::~ComputeCommandEncoderVulkan() {
    if (has_label_) {
        pop_label();
    }
    base_encoder_->cmd_buffer_ = cmd_buffer_;
}

auto ComputeCommandEncoderVulkan::push_label(CommandLabel const& label) -> void {
    push_label_impl(cmd_buffer_, label);
}

auto ComputeCommandEncoderVulkan::pop_label() -> void {
    pop_label_impl(cmd_buffer_);
}

auto ComputeCommandEncoderVulkan::set_pipeline(CRef<ComputePipeline> pipeline) -> void {
    curr_pipeline_ = pipeline.cast_to<const ComputePipelineVulkan>().get();
    vkCmdBindPipeline(cmd_buffer_, VK_PIPELINE_BIND_POINT_COMPUTE, curr_pipeline_->raw());

    if (auto embedded_samplers_set = curr_pipeline_->static_samplers_set(); embedded_samplers_set != ~0u) {
        vkCmdBindDescriptorBufferEmbeddedSamplersEXT(
            cmd_buffer_, VK_PIPELINE_BIND_POINT_COMPUTE, curr_pipeline_->raw_layout(), embedded_samplers_set
        );
    }
}

auto ComputeCommandEncoderVulkan::set_descriptors(
    uint32_t from_group_index, CSpan<DescriptorHandle> descriptors
) -> void {
    base_encoder_->set_descriptors(
        cmd_buffer_, VK_PIPELINE_BIND_POINT_COMPUTE, curr_pipeline_->raw_layout(), from_group_index, descriptors
    );
}

auto ComputeCommandEncoderVulkan::push_constants(void const* data, uint32_t size, uint32_t offset) -> void {
    vkCmdPushConstants(cmd_buffer_, curr_pipeline_->raw_layout(), VK_SHADER_STAGE_COMPUTE_BIT, offset, size, data);
}

void ComputeCommandEncoderVulkan::dispatch(uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z) {
    vkCmdDispatch(cmd_buffer_, num_groups_x, num_groups_y, num_groups_z);
}

}
