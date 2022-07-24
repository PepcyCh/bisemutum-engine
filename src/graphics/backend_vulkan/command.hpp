#pragma once

#include <volk.h>

#include "graphics/command.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class DeviceVulkan;

class CommandBufferVulkan : public CommandBuffer {
public:
    CommandBufferVulkan(DeviceVulkan *device, VkCommandBuffer cmd_buffer);

    VkCommandBuffer Raw() const { return cmd_buffer_; }

private:
    DeviceVulkan *device_;
    VkCommandBuffer cmd_buffer_;
};

class RenderCommandEncoderVulkan;
class ComputeCommandEncoderVulkan;

class CommandEncoderVulkan : public CommandEncoder {
public:
    CommandEncoderVulkan(DeviceVulkan *device, VkCommandBuffer cmd_buffer);
    ~CommandEncoderVulkan();

    Ptr<CommandBuffer> Finish() override;

    void PushLabel(const CommandLabel &label) override;

    void PopLabel() override;

    void CopyBufferToBuffer(Buffer *src_buffer, Buffer *dst_buffer, Span<BufferCopyDesc> regions) override;

    void CopyTextureToTexture(Texture *src_texture, Texture *dst_texture, Span<TextureCopyDesc> regions) override;

    void CopyBufferToTexture(Buffer *src_buffer, Texture *dst_texture, Span<BufferTextureCopyDesc> regions) override;

    void CopyTextureToBuffer(Texture *src_texture, Buffer *dst_buffer, Span<BufferTextureCopyDesc> regions) override;

    Ptr<RenderCommandEncoder> BeginRenderPass(const CommandLabel &label, const RenderTargetDesc &desc) override;

    Ptr<ComputeCommandEncoder> BeginComputePass(const CommandLabel &label) override;

private:
    friend RenderCommandEncoderVulkan;
    friend ComputeCommandEncoderVulkan;

    DeviceVulkan *device_;
    VkCommandBuffer cmd_buffer_;
};

class RenderCommandEncoderVulkan : public RenderCommandEncoder {
public:
    RenderCommandEncoderVulkan(DeviceVulkan *device, const RenderTargetDesc &desc, CommandEncoderVulkan *base_encoder,
        const std::string &label);
    ~RenderCommandEncoderVulkan();

    Ptr<CommandBuffer> Finish() override;

    void PushLabel(const CommandLabel &label) override;

    void PopLabel() override;

    void CopyBufferToBuffer(Buffer *src_buffer, Buffer *dst_buffer, Span<BufferCopyDesc> regions) override;

    void CopyTextureToTexture(Texture *src_texture, Texture *dst_texture, Span<TextureCopyDesc> regions) override;

    void CopyBufferToTexture(Buffer *src_buffer, Texture *dst_texture, Span<BufferTextureCopyDesc> regions) override;

    void CopyTextureToBuffer(Texture *src_texture, Buffer *dst_buffer, Span<BufferTextureCopyDesc> regions) override;

    void BindVertexBuffer(Span<std::pair<Buffer *, uint64_t>> buffers, uint32_t first_binding = 0) override;
    void BindIndexBuffer(Buffer *buffer, uint64_t offset, IndexType index_type) override;

    void Draw(uint32_t num_vertices, uint32_t num_instance = 1,
        uint32_t first_vertex = 0, uint32_t first_instance = 0) override;
    void DrawIndexed(uint32_t num_indices, uint32_t num_instance = 1,
        uint32_t first_index = 0, uint32_t vertex_offset = 0, uint32_t first_instance = 0) override;

private:
    DeviceVulkan *device_;
    CommandEncoderVulkan *base_encoder_;
    std::string label_;
    VkCommandBuffer cmd_buffer_;

    VkRenderPass render_pass_;
    VkFramebuffer frame_buffer_;
};

class ComputeCommandEncoderVulkan : public ComputeCommandEncoder {
public:
    ComputeCommandEncoderVulkan(DeviceVulkan *device, CommandEncoderVulkan *base_encoder, const std::string &label);
    ~ComputeCommandEncoderVulkan();

    Ptr<CommandBuffer> Finish() override;

    void PushLabel(const CommandLabel &label) override;

    void PopLabel() override;

    void CopyBufferToBuffer(Buffer *src_buffer, Buffer *dst_buffer, Span<BufferCopyDesc> regions) override;

    void CopyTextureToTexture(Texture *src_texture, Texture *dst_texture, Span<TextureCopyDesc> regions) override;

    void CopyBufferToTexture(Buffer *src_buffer, Texture *dst_texture, Span<BufferTextureCopyDesc> regions) override;

    void CopyTextureToBuffer(Texture *src_texture, Buffer *dst_buffer, Span<BufferTextureCopyDesc> regions) override;

    void Dispatch(uint32_t x, uint32_t y, uint32_t z) override;

private:
    DeviceVulkan *device_;
    CommandEncoderVulkan *base_encoder_;
    std::string label_;
    VkCommandBuffer cmd_buffer_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
