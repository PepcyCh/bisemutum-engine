#pragma once

#include <optional>

#include "core/span.hpp"
#include "descriptor.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

struct CommandLabel {
    std::string label;
    struct {
        float r = 0.0f;
        float g = 0.0f;
        float b = 1.0f;
    } color;
};

struct BufferCopyDesc {
    uint64_t src_offset = 0;
    uint64_t dst_offset = 0;
    uint64_t length = ~0ull;
};

struct TextureCopyDesc {
    Offset3D src_offset = {};
    Offset3D dst_offset = {};
    Extent3D extent = { ~0u, ~0u, ~0u };
    uint32_t src_level = 0;
    uint32_t dst_level = 0;
};

struct BufferTextureCopyDesc {
    uint64_t buffer_offset = 0;
    uint32_t buffer_bytes_per_row = 0;
    uint32_t buffer_rows_per_texture = 0;
    Offset3D texture_offset = { 0, 0, 0 };
    Extent3D texture_extent = { ~0u, ~0u, ~0u };
    uint32_t texture_level = 0;
};

struct DepthStencilAttachmentDesc {
    TextureView texture;
    float clear_depth = 0.0f;
    uint8_t clear_stencil = 0;
    bool clear = false;
    bool store = true;
};
struct ColorAttachmentDesc {
    TextureView texture;
    std::optional<TextureView> resolve;
    struct {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 0.0f;
    } clear_color;
    bool clear = false;
    bool store = true;
};
inline constexpr uint32_t kMaxRenderTargetsCount = 8;
struct RenderTargetDesc {
    Vec<ColorAttachmentDesc> colors = {};
    std::optional<DepthStencilAttachmentDesc> depth_stencil = std::nullopt;
};

struct BufferBarrier {
    Ref<Buffer> buffer;
    BitFlags<ResourceAccessType> src_access_type;
    BitFlags<ResourceAccessType> dst_access_type;
    class Queue *src_queue = nullptr;
    class Queue *dst_queue = nullptr;
};
struct TextureBarrier {
    TextureView texture;
    BitFlags<ResourceAccessType> src_access_type;
    BitFlags<ResourceAccessType> dst_access_type;
    class Queue *src_queue = nullptr;
    class Queue *dst_queue = nullptr;
};

struct Viewport {
    float x = 0.0f;
    float y = 0.0f;
    float width;
    float height;
    float min_depth = 0.0f;
    float max_depth = 1.0f;
};
struct Scissor {
    int x = 0;
    int y = 0;
    uint32_t width;
    uint32_t height;
};

class CommandEncoder;
class RenderCommandEncoder;
class ComputeCommandEncoder;

class CommandBuffer {
public:
    virtual ~CommandBuffer() = default;

protected:
    CommandBuffer() = default;
};

class CommandEncoderBase {
public:
    virtual ~CommandEncoderBase() = default;

    virtual Ptr<CommandBuffer> Finish() = 0;

    virtual void PushLabel(const CommandLabel &label) = 0;
    virtual void PopLabel() = 0;

protected:
    CommandEncoderBase() = default;
};

class CommandEncoder : public CommandEncoderBase {
public:
    virtual ~CommandEncoder() = default;

    virtual void CopyBufferToBuffer(Ref<Buffer> src_buffer, Ref<Buffer> dst_buffer,
        Span<BufferCopyDesc> regions = { {} }) = 0;

    virtual void CopyTextureToTexture(Ref<Texture> src_texture, Ref<Texture> dst_texture,
        Span<TextureCopyDesc> regions = { {} }) = 0;

    virtual void CopyBufferToTexture(Ref<Buffer> src_buffer, Ref<Texture> dst_texture,
        Span<BufferTextureCopyDesc> regions) = 0;

    virtual void CopyTextureToBuffer(Ref<Texture> src_texture, Ref<Buffer> dst_buffer,
        Span<BufferTextureCopyDesc> regions) = 0;

    virtual void ResourceBarrier(Span<BufferBarrier> buffer_barriers, Span<TextureBarrier> texture_barriers) = 0;

    virtual Ptr<RenderCommandEncoder> BeginRenderPass(const CommandLabel &label, const RenderTargetDesc &desc) = 0;

    virtual Ptr<ComputeCommandEncoder> BeginComputePass(const CommandLabel &label) = 0;

protected:
    CommandEncoder() = default;
};

class RenderCommandEncoder : public CommandEncoderBase {
public:
    virtual ~RenderCommandEncoder() = default;

    virtual void SetPipeline(Ref<class RenderPipeline> pipeline) = 0;

    virtual void BindShaderParams(uint32_t set_index, const ShaderParams &values) = 0;

    virtual void PushConstants(const void *data, uint32_t size, uint32_t offset = 0) = 0;
    template <typename T>
    void PushConstants(const T &data) { PushConstants(&data, sizeof(T)); }

    virtual void SetViewports(Span<Viewport> viewports) = 0;
    virtual void SetScissors(Span<Scissor> scissors) = 0;

    virtual void BindVertexBuffer(Span<BufferRange> buffers, uint32_t first_binding = 0) = 0;
    virtual void BindIndexBuffer(Ref<Buffer> buffer, uint64_t offset, IndexType index_type) = 0;

    virtual void Draw(uint32_t num_vertices, uint32_t num_instance = 1,
        uint32_t first_vertex = 0, uint32_t first_instance = 0) = 0;
    virtual void DrawIndexed(uint32_t num_indices, uint32_t num_instance = 1,
        uint32_t first_index = 0, uint32_t vertex_offset = 0, uint32_t first_instance = 0) = 0;

protected:
    RenderCommandEncoder() = default;
};

class ComputeCommandEncoder : public CommandEncoderBase {
public:
    virtual ~ComputeCommandEncoder() = default;

    virtual void SetPipeline(Ref<class ComputePipeline> pipeline) = 0;

    virtual void BindShaderParams(uint32_t set_index, const ShaderParams &values) = 0;

    virtual void PushConstants(const void *data, uint32_t size, uint32_t offset = 0) = 0;
    template <typename T>
    void PushConstants(const T &data) { PushConstants(&data, sizeof(T)); }

    virtual void Dispatch(uint32_t size_x, uint32_t size_y, uint32_t size_z) = 0;

protected:
    ComputeCommandEncoder() = default;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
