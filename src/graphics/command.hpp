#pragma once

#include <optional>

#include "core/span.hpp"
#include "shader.hpp"

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
    Offset3D texture_offset = {};
    Extent3D texture_extent = {};
    uint32_t texture_level = 0;
};

struct DepthStencilAttachmentDesc {
    TextureRange texture;
    float clear_depth = 0.0f;
    uint32_t clear_stencil = 0;
    bool clear = false;
    bool store = true;
};
struct ColorAttachmentDesc {
    TextureRange texture;
    std::optional<TextureRange> resolve;
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
    Vec<ColorAttachmentDesc> colors;
    std::optional<DepthStencilAttachmentDesc> depth_stencil;
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

    virtual void CopyBufferToBuffer(Ref<Buffer> src_buffer, Ref<Buffer> dst_buffer, Span<BufferCopyDesc> regions) = 0;

    virtual void CopyTextureToTexture(Ref<Texture> src_texture, Ref<Texture> dst_texture, Span<TextureCopyDesc> regions) = 0;

    virtual void CopyBufferToTexture(Ref<Buffer> src_buffer, Ref<Texture> dst_texture, Span<BufferTextureCopyDesc> regions) = 0;

    virtual void CopyTextureToBuffer(Ref<Texture> src_texture, Ref<Buffer> dst_buffer, Span<BufferTextureCopyDesc> regions) = 0;

    virtual Ptr<RenderCommandEncoder> BeginRenderPass(const CommandLabel &label, const RenderTargetDesc &desc) = 0;

    virtual Ptr<ComputeCommandEncoder> BeginComputePass(const CommandLabel &label) = 0;

protected:
    CommandEncoder() = default;
};

struct VertexBufferDesc {
    Ref<Buffer> buffer;
    uint64_t offset = 0;
    uint32_t stride = 0;
};

class RenderCommandEncoder : public CommandEncoderBase {
public:
    virtual ~RenderCommandEncoder() = default;

    virtual void BindVertexBuffer(Span<VertexBufferDesc> buffers, uint32_t first_binding = 0) = 0;
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

    virtual void Dispatch(uint32_t x, uint32_t y, uint32_t z) = 0;

protected:
    ComputeCommandEncoder() = default;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
