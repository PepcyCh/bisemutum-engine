#pragma once

#include <volk.h>

#include "graphics/command.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class DeviceVulkan;
class FrameContextVulkan;

class CommandBufferVulkan : public CommandBuffer {
public:
    CommandBufferVulkan(Ref<DeviceVulkan> device, VkCommandBuffer cmd_buffer);

    VkCommandBuffer Raw() const { return cmd_buffer_; }

private:
    Ref<DeviceVulkan> device_;
    VkCommandBuffer cmd_buffer_;
};

class RenderCommandEncoderVulkan;
class ComputeCommandEncoderVulkan;

class CommandEncoderVulkan : public CommandEncoder, public RefFromThis<CommandEncoderVulkan> {
public:
    CommandEncoderVulkan(Ref<DeviceVulkan> device, Ref<FrameContextVulkan> context, VkCommandBuffer cmd_buffer);
    ~CommandEncoderVulkan();

    Ptr<CommandBuffer> Finish() override;

    void PushLabel(const CommandLabel &label) override;

    void PopLabel() override;

    void CopyBufferToBuffer(Ref<Buffer> src_buffer, Ref<Buffer> dst_buffer, Span<BufferCopyDesc> regions) override;

    void CopyTextureToTexture(Ref<Texture> src_texture, Ref<Texture> dst_texture,
        Span<TextureCopyDesc> regions) override;

    void CopyBufferToTexture(Ref<Buffer> src_buffer, Ref<Texture> dst_texture,
        Span<BufferTextureCopyDesc> regions) override;

    void CopyTextureToBuffer(Ref<Texture> src_texture, Ref<Buffer> dst_buffer,
        Span<BufferTextureCopyDesc> regions) override;

    void ResourceBarrier(Span<BufferBarrier> buffer_barriers, Span<TextureBarrier> texture_barriers) override;

    Ptr<RenderCommandEncoder> BeginRenderPass(const CommandLabel &label, const RenderTargetDesc &desc) override;

    Ptr<ComputeCommandEncoder> BeginComputePass(const CommandLabel &label) override;

private:
    friend RenderCommandEncoderVulkan;
    friend ComputeCommandEncoderVulkan;

    Ref<DeviceVulkan> device_;
    Ref<FrameContextVulkan> context_;
    VkCommandBuffer cmd_buffer_;
};

class RenderCommandEncoderVulkan : public RenderCommandEncoder {
public:
    RenderCommandEncoderVulkan(Ref<DeviceVulkan> device, const RenderTargetDesc &desc,
        Ref<CommandEncoderVulkan> base_encoder, const std::string &label);
    ~RenderCommandEncoderVulkan();

    Ptr<CommandBuffer> Finish() override;

    void PushLabel(const CommandLabel &label) override;

    void PopLabel() override;

    void SetPipeline(Ref<class RenderPipeline> pipeline) override;

    void BindShaderParams(uint32_t set_index, const ShaderParams &values) override;

    void PushConstants(const void *data, uint32_t size, uint32_t offset = 0) override;

    void BindVertexBuffer(Span<BufferRange> buffers, uint32_t first_binding = 0) override;
    void BindIndexBuffer(Ref<Buffer> buffer, uint64_t offset, IndexType index_type) override;

    void Draw(uint32_t num_vertices, uint32_t num_instance = 1,
        uint32_t first_vertex = 0, uint32_t first_instance = 0) override;
    void DrawIndexed(uint32_t num_indices, uint32_t num_instance = 1,
        uint32_t first_index = 0, uint32_t vertex_offset = 0, uint32_t first_instance = 0) override;

private:
    Ref<DeviceVulkan> device_;
    Ref<CommandEncoderVulkan> base_encoder_;
    std::string label_;
    VkCommandBuffer cmd_buffer_;

    class RenderPipelineVulkan *curr_pipeline_ = nullptr;
};

class ComputeCommandEncoderVulkan : public ComputeCommandEncoder {
public:
    ComputeCommandEncoderVulkan(Ref<DeviceVulkan> device, Ref<CommandEncoderVulkan> base_encoder, const std::string &label);
    ~ComputeCommandEncoderVulkan();

    Ptr<CommandBuffer> Finish() override;

    void PushLabel(const CommandLabel &label) override;

    void PopLabel() override;

    void SetPipeline(Ref<class ComputePipeline> pipeline) override;

    void BindShaderParams(uint32_t set_index, const ShaderParams &values) override;

    void PushConstants(const void *data, uint32_t size, uint32_t offset = 0) override;

    void Dispatch(uint32_t size_x, uint32_t size_y, uint32_t size_z) override;

private:
    Ref<DeviceVulkan> device_;
    Ref<CommandEncoderVulkan> base_encoder_;
    std::string label_;
    VkCommandBuffer cmd_buffer_;

    class ComputePipelineVulkan *curr_pipeline_ = nullptr;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
