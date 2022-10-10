#pragma once

#include "graphics/command.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class RenderGraph;

struct BufferHandle {
private:
    friend RenderGraph;

    size_t node_index_;
};
struct TextureHandle {
private:
    friend RenderGraph;

    size_t node_index_;
};

enum class BufferReadType : uint8_t {
    eUniform,
    eStorage,
};
struct PassReadBuffer {
    BufferHandle handle;
    BufferReadType type;
};
struct PassWriteBuffer {
    BufferHandle handle;
};
struct PassReadTexture {
    TextureHandle handle;
};
struct PassWriteTexture {
    TextureHandle handle;
    bool generate_mipmaps;
};
class PassResource {
public:
    Ref<gfx::Buffer> Buffer(const std::string &name) const;
    Ref<gfx::Texture> Texture(const std::string &name) const;

private:
    friend RenderGraph;

    void GetShaderBarriers(RenderGraph &rg, bool is_render_pass,
        Vec<gfx::BufferBarrier> &buffer_barriers, Vec<gfx::TextureBarrier> &texture_barriers);

    const RenderGraph *graph_;
    HashMap<std::string, PassReadBuffer> read_buffers_;
    HashMap<std::string, PassWriteBuffer> write_buffers_;
    HashMap<std::string, PassReadTexture> read_textures_;
    HashMap<std::string, PassWriteTexture> write_textures_;
};

struct RenderPassColorTarget {
    TextureHandle handle;
    uint32_t base_layer = 0;
    uint32_t layers = 1;
    uint32_t level = 0;
    struct {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 0.0f;
    } clear_color;
    bool clear = false;
    bool store = true;
    bool generate_mipmaps = false;
};
class RenderPassColorTargetBuilder {
public:
    RenderPassColorTargetBuilder(TextureHandle handle);

    operator RenderPassColorTarget() const { return target_; }

    RenderPassColorTargetBuilder &ArrayLayer(uint32_t base_layer, uint32_t layers = 1);
    RenderPassColorTargetBuilder &MipLevel(uint32_t level);
    RenderPassColorTargetBuilder &ClearColor(float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 1.0f);
    RenderPassColorTargetBuilder &DontStore();
    RenderPassColorTargetBuilder &GenerateMipmaps();

private:
    RenderPassColorTarget target_;
};

struct RenderPassDepthStencilTarget {
    TextureHandle handle;
    uint32_t base_layer = 0;
    uint32_t layers = 1;
    uint32_t level = 0;
    float clear_depth = 1.0f;
    uint8_t clear_stencil = 0;
    bool clear = false;
    bool store = true;
    bool generate_mipmaps = false;
};
struct RenderPassDepthStencilTargetBuilder {
public:
    RenderPassDepthStencilTargetBuilder(TextureHandle handle);

    operator RenderPassDepthStencilTarget() const { return target_; }

    RenderPassDepthStencilTargetBuilder &ArrayLayer(uint32_t base_layer, uint32_t layers = 1);
    RenderPassDepthStencilTargetBuilder &MipLevel(uint32_t level);
    RenderPassDepthStencilTargetBuilder &ClearDepthStencil(float depth = 1.0f, uint8_t stencil = 0);
    RenderPassDepthStencilTargetBuilder &DontStore();
    RenderPassDepthStencilTargetBuilder &GenerateMipmaps();

private:
    RenderPassDepthStencilTarget target_;
};

class RenderPassBuilder {
public:
    RenderPassBuilder &Color(uint32_t index, const RenderPassColorTargetBuilder &target);
    RenderPassBuilder &DepthStencil(const RenderPassDepthStencilTargetBuilder &target);

    RenderPassBuilder &Read(const std::string &name, BufferHandle handle, BufferReadType type);
    RenderPassBuilder &Read(const std::string &name, TextureHandle handle);

    RenderPassBuilder &Write(const std::string &name, BufferHandle handle);
    RenderPassBuilder &Write(const std::string &name, TextureHandle handle, bool generate_mipmaps = false);

private:
    friend RenderGraph;

    std::optional<RenderPassColorTarget> color_targets_[gfx::kMaxRenderTargetsCount];
    std::optional<RenderPassDepthStencilTarget> depth_stencil_target_;

    HashMap<std::string, PassReadBuffer> read_buffers_;
    HashMap<std::string, PassWriteBuffer> write_buffers_;
    HashMap<std::string, PassReadTexture> read_textures_;
    HashMap<std::string, PassWriteTexture> write_textures_;
};

class ComputePassBuilder {
public:
    ComputePassBuilder &Read(const std::string &name, BufferHandle handle, BufferReadType type);
    ComputePassBuilder &Read(const std::string &name, TextureHandle handle);

    ComputePassBuilder &Write(const std::string &name, BufferHandle handle);
    ComputePassBuilder &Write(const std::string &name, TextureHandle handle, bool generate_mipmaps = false);

private:
    friend RenderGraph;

    HashMap<std::string, PassReadBuffer> read_buffers_;
    HashMap<std::string, PassWriteBuffer> write_buffers_;
    HashMap<std::string, PassReadTexture> read_textures_;
    HashMap<std::string, PassWriteTexture> write_textures_;
};

class BlitPassBuilder {
public:
    BlitPassBuilder &Src(TextureHandle handle, uint32_t level = 0, uint32_t layer = 0);
    BlitPassBuilder &Dst(TextureHandle handle, uint32_t level = 0, uint32_t layer = 0);

private:
    friend RenderGraph;

    TextureHandle src_handle_;
    uint32_t src_level_;
    uint32_t src_layer_;
    TextureHandle dst_handle_;
    uint32_t dst_level_;
    uint32_t dst_layer_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
