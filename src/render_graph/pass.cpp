#include "pass.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFXRG_NAMESPACE_BEGIN

RenderPassColorTargetBuilder &RenderPassColorTargetBuilder::ArrayLayer(uint32_t base_layer, uint32_t layers) {
    target_.base_layer = base_layer;
    target_.layers = layers;
    return *this;
}

RenderPassColorTargetBuilder &RenderPassColorTargetBuilder::MipLevel(uint32_t level) {
    target_.level = level;
    return *this;
}

RenderPassColorTargetBuilder &RenderPassColorTargetBuilder::ClearColor(float r, float g, float b, float a) {
    target_.clear_color = { r, g, b, a };
    target_.clear = true;
    return *this;
}

RenderPassColorTargetBuilder &RenderPassColorTargetBuilder::DontStore() {
    target_.store = false;
    return *this;
}

RenderPassDepthStencilTargetBuilder &RenderPassDepthStencilTargetBuilder::ArrayLayer(
    uint32_t base_layer, uint32_t layers) {
    target_.base_layer = base_layer;
    target_.layers = layers;
    return *this;
}

RenderPassDepthStencilTargetBuilder &RenderPassDepthStencilTargetBuilder::MipLevel(uint32_t level) {
    target_.level = level;
    return *this;
}

RenderPassDepthStencilTargetBuilder &RenderPassDepthStencilTargetBuilder::ClearDepthStencil(
    float depth, uint8_t stencil) {
    target_.clear_depth = depth;
    target_.clear_stencil = stencil;
    target_.clear = true;
    return *this;
}

RenderPassDepthStencilTargetBuilder &RenderPassDepthStencilTargetBuilder::DontStore() {
    target_.store = false;
    return *this;
}

RenderPassBuilder &RenderPassBuilder::Color(uint32_t index, const RenderPassColorTargetBuilder &target) {
    if (index < gfx::kMaxRenderTargetsCount) {
        color_targets_[index] = target;
    }
    return *this;
}

RenderPassBuilder &RenderPassBuilder::DepthStencil(const RenderPassDepthStencilTargetBuilder &target) {
    depth_stencil_target_ = target;
    return *this;
}

RenderPassBuilder &RenderPassBuilder::Read(const std::string &name, BufferHandle handle, BufferReadType type) {
    read_buffers_[name] = handle;
    read_buffers_type_[name] = type;
    return *this;
}

RenderPassBuilder &RenderPassBuilder::Read(const std::string &name, TextureHandle handle) {
    read_textures_[name] = handle;
    return *this;
}

RenderPassBuilder &RenderPassBuilder::Write(const std::string &name, BufferHandle handle) {
    write_buffers_[name] = handle;
    return *this;
}

RenderPassBuilder &RenderPassBuilder::Write(const std::string &name, TextureHandle handle) {
    write_textures_[name] = handle;
    return *this;
}

ComputePassBuilder &ComputePassBuilder::Read(const std::string &name, BufferHandle handle, BufferReadType type) {
    read_buffers_[name] = handle;
    read_buffers_type_[name] = type;
    return *this;
}

ComputePassBuilder &ComputePassBuilder::Read(const std::string &name, TextureHandle handle) {
    read_textures_[name] = handle;
    return *this;
}

ComputePassBuilder &ComputePassBuilder::Write(const std::string &name, BufferHandle handle) {
    write_buffers_[name] = handle;
    return *this;
}

ComputePassBuilder &ComputePassBuilder::Write(const std::string &name, TextureHandle handle) {
    write_textures_[name] = handle;
    return *this;
}

BISMUTH_GFXRG_NAMESPACE_END

BISMUTH_NAMESPACE_END
