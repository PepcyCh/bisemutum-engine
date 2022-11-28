#pragma once

#include "utils.hpp"
#include "graphics/command.hpp"
#include "context.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class DeviceD3D12;

class CommandBufferD3D12 final : public CommandBuffer {
public:
    CommandBufferD3D12(Ref<DeviceD3D12> device, ID3D12GraphicsCommandList4 *cmd_list);

    ID3D12GraphicsCommandList4 *Raw() const { return cmd_list_; }

private:
    Ref<DeviceD3D12> device_;
    ID3D12GraphicsCommandList4 *cmd_list_;
};

class RenderCommandEncoderD3D12;
class ComputeCommandEncoderD3D12;

class CommandEncoderD3D12 final : public CommandEncoder, public RefFromThis<CommandEncoderD3D12> {
public:
    CommandEncoderD3D12(Ref<DeviceD3D12> device, Ref<FrameContextD3D12> context,
        ID3D12GraphicsCommandList4 *cmd_list);
    ~CommandEncoderD3D12();

    Ptr<CommandBuffer> Finish() override;

    void PushLabel(const CommandLabel &label) override;

    void PopLabel() override;

    void CopyBufferToBuffer(Ref<Buffer> src_buffer, Ref<Buffer> dst_buffer,
        Span<BufferCopyDesc> regions = { {} }) override;

    void CopyTextureToTexture(Ref<Texture> src_texture, Ref<Texture> dst_texture,
        Span<TextureCopyDesc> regions = { {} }) override;

    void CopyBufferToTexture(Ref<Buffer> src_buffer, Ref<Texture> dst_texture,
        Span<BufferTextureCopyDesc> regions) override;

    void CopyTextureToBuffer(Ref<Texture> src_texture, Ref<Buffer> dst_buffer,
        Span<BufferTextureCopyDesc> regions) override;

    void ResourceBarrier(Span<BufferBarrier> buffer_barriers, Span<TextureBarrier> texture_barriers) override;

    Ptr<RenderCommandEncoder> BeginRenderPass(const CommandLabel &label, const RenderTargetDesc &desc) override;

    Ptr<ComputeCommandEncoder> BeginComputePass(const CommandLabel &label) override;

private:
    friend RenderCommandEncoderD3D12;
    friend ComputeCommandEncoderD3D12;

    Ref<DeviceD3D12> device_;
    Ref<FrameContextD3D12> context_;
    ID3D12GraphicsCommandList4 *cmd_list_;
};

class RenderCommandEncoderD3D12 final : public RenderCommandEncoder {
public:
    RenderCommandEncoderD3D12(Ref<DeviceD3D12> device, const RenderTargetDesc &desc,
        Ref<CommandEncoderD3D12> base_encoder, const std::string &label);
    ~RenderCommandEncoderD3D12();

    Ptr<CommandBuffer> Finish() override;

    void PushLabel(const CommandLabel &label) override;

    void PopLabel() override;

    void SetPipeline(Ref<class RenderPipeline> pipeline) override;

    void BindShaderParams(uint32_t set_index, const ShaderParams &values) override;

    void PushConstants(const void *data, uint32_t size, uint32_t offset = 0) override;

    void SetViewports(Span<Viewport> viewports) override;
    void SetScissors(Span<Scissor> scissors) override;

    void BindVertexBuffer(Span<BufferRange> buffers, uint32_t first_binding = 0) override;
    void BindIndexBuffer(Ref<Buffer> buffer, uint64_t offset, IndexType index_type) override;

    void Draw(uint32_t num_vertices, uint32_t num_instance = 1,
        uint32_t first_vertex = 0, uint32_t first_instance = 0) override;
    void DrawIndexed(uint32_t num_indices, uint32_t num_instance = 1,
        uint32_t first_index = 0, uint32_t vertex_offset = 0, uint32_t first_instance = 0) override;

private:
    Ref<DeviceD3D12> device_;
    Ref<CommandEncoderD3D12> base_encoder_;
    std::string label_;
    ID3D12GraphicsCommandList4 *cmd_list_;

    Vec<ResourceFormat> color_formats_;
    ResourceFormat depth_stencil_format_;

    class RenderPipelineD3D12 *curr_pipeline_ = nullptr;
};

class ComputeCommandEncoderD3D12 final : public ComputeCommandEncoder {
public:
    ComputeCommandEncoderD3D12(Ref<DeviceD3D12> device, Ref<CommandEncoderD3D12> base_encoder, const std::string &label);
    ~ComputeCommandEncoderD3D12();

    Ptr<CommandBuffer> Finish() override;

    void PushLabel(const CommandLabel &label) override;

    void PopLabel() override;

    void SetPipeline(Ref<class ComputePipeline> pipeline) override;

    void BindShaderParams(uint32_t set_index, const ShaderParams &values) override;

    void PushConstants(const void *data, uint32_t size, uint32_t offset = 0) override;

    void Dispatch(uint32_t size_x, uint32_t size_y, uint32_t size_z) override;

private:
    Ref<DeviceD3D12> device_;
    Ref<CommandEncoderD3D12> base_encoder_;
    std::string label_;
    ID3D12GraphicsCommandList4 *cmd_list_;

    class ComputePipelineD3D12 *curr_pipeline_ = nullptr;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
