#pragma once

#include <vulkan/vulkan.h>
#include <bisemutum/rhi/command.hpp>

namespace bi::rhi {

struct DeviceVulkan;

struct CommandPoolVulkan final : CommandPool {
    CommandPoolVulkan(Ref<DeviceVulkan> device, CommandPoolDesc const& desc);
    ~CommandPoolVulkan() override;

    auto reset() -> void override;

    auto get_command_encoder() -> Box<CommandEncoder> override;

private:
    Ref<DeviceVulkan> device_;
    VkCommandPool cmd_pool_;

    std::vector<VkCommandBuffer> allocated_cmd_buffers_;
    size_t available_cmd_buffer_index_ = 0;
};

struct CommandBufferVulkan final : CommandBuffer {
    CommandBufferVulkan(Ref<DeviceVulkan> device, VkCommandBuffer cmd_buffer);

    auto raw() const -> VkCommandBuffer { return cmd_buffer_; }

private:
    Ref<DeviceVulkan> device_;
    VkCommandBuffer cmd_buffer_ = VK_NULL_HANDLE;
};

struct GraphicsCommandEncoderVulkan;
struct ComputeCommandEncoderVulkan;

struct CommandEncoderVulkan final : CommandEncoder {
    CommandEncoderVulkan(Ref<DeviceVulkan> device, VkCommandBuffer cmd_buffer);
    ~CommandEncoderVulkan();

    auto finish() -> Box<CommandBuffer> override;

    auto push_label(CommandLabel const& label) -> void override;

    auto pop_label() -> void override;

    auto copy_buffer_to_buffer(
        CRef<Buffer> src_buffer,
        Ref<Buffer> dst_buffer,
        BufferCopyDesc const& region
    ) -> void override;

    auto copy_texture_to_texture(
        CRef<Texture> src_texture,
        Ref<Texture> dst_texture,
        TextureCopyDesc const& region
    ) -> void override;

    auto copy_buffer_to_texture(
        CRef<Buffer> src_buffer,
        Ref<Texture> dst_texture,
        BufferTextureCopyDesc const& region
    ) -> void override;

    auto copy_texture_to_buffer(
        CRef<Texture> src_texture,
        Ref<Buffer> dst_buffer,
        BufferTextureCopyDesc const& region
    ) -> void override;

    auto resource_barriers(
        CSpan<BufferBarrier> buffer_barriers, CSpan<TextureBarrier> texture_barriers
    ) -> void override;

    auto set_descriptor_heaps(CSpan<Ref<DescriptorHeap>> heaps) -> void override;

    auto begin_render_pass(
        CommandLabel const& label, RenderTargetDesc const& desc
    ) -> Box<GraphicsCommandEncoder> override;

    auto begin_compute_pass(CommandLabel const& label) -> Box<ComputeCommandEncoder> override;

private:
    auto set_descriptors(
        VkCommandBuffer cmd_buffer, VkPipelineBindPoint bind_point, VkPipelineLayout pipline_layout,
        uint32_t from_group_index, CSpan<DescriptorHandle> descriptors
    ) const -> void;

    friend GraphicsCommandEncoderVulkan;
    friend ComputeCommandEncoderVulkan;

    Ref<DeviceVulkan> device_;
    VkCommandBuffer cmd_buffer_;

    std::vector<uint64_t> binded_descriptor_heaps_start_;
};

struct GraphicsCommandEncoderVulkan final : GraphicsCommandEncoder {
    GraphicsCommandEncoderVulkan(
        Ref<DeviceVulkan> device,
        RenderTargetDesc const& desc,
        Ref<CommandEncoderVulkan> base_encoder,
        bool has_label
    );
    ~GraphicsCommandEncoderVulkan();

    auto push_label(CommandLabel const& label) -> void override;

    auto pop_label() -> void override;

    auto set_pipeline(CRef<GraphicsPipeline> pipeline) -> void override;

    auto set_descriptors(uint32_t from_group_index, CSpan<DescriptorHandle> descriptors) -> void override;

    auto push_constants(void const* data, uint32_t size, uint32_t offset) -> void override;

    void set_viewports(CSpan<Viewport> viewports) override;
    void set_scissors(CSpan<Scissor> scissors) override;

    auto set_vertex_buffer(
        CSpan<Ref<Buffer>> buffers, CSpan<uint64_t> offsets, uint32_t first_binding
    ) -> void override;
    auto set_index_buffer(
        Ref<Buffer> buffer, uint64_t offset, IndexType index_type
    ) -> void override;

    auto draw(
        uint32_t num_vertices,
        uint32_t num_instance,
        uint32_t first_vertex,
        uint32_t first_instance
    ) -> void override;
    auto draw_indexed(
        uint32_t num_indices,
        uint32_t num_instance,
        uint32_t first_index,
        uint32_t vertex_offset,
        uint32_t first_instance
    ) -> void override;

private:
    Ref<DeviceVulkan> device_;
    Ref<CommandEncoderVulkan> base_encoder_;
    VkCommandBuffer cmd_buffer_;
    bool has_label_;

    struct GraphicsPipelineVulkan const* curr_pipeline_ = nullptr;
};

struct ComputeCommandEncoderVulkan final : ComputeCommandEncoder {
    ComputeCommandEncoderVulkan(Ref<DeviceVulkan> device, Ref<CommandEncoderVulkan> base_encoder, bool has_label);
    ~ComputeCommandEncoderVulkan();

    auto push_label(CommandLabel const& label) -> void override;

    auto pop_label() -> void override;

    auto set_pipeline(CRef<ComputePipeline> pipeline) -> void override;

    auto set_descriptors(uint32_t from_group_index, CSpan<DescriptorHandle> descriptors) -> void override;

    auto push_constants(void const* data, uint32_t size, uint32_t offset) -> void override;

    void dispatch(uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z) override;

private:
    Ref<DeviceVulkan> device_;
    Ref<CommandEncoderVulkan> base_encoder_;
    VkCommandBuffer cmd_buffer_;
    bool has_label_;

    struct ComputePipelineVulkan const* curr_pipeline_ = nullptr;
};

}
