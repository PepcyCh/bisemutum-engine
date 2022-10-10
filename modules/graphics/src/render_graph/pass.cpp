#include "render_graph/pass.hpp"

#include "render_graph/graph.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

Ref<gfx::Buffer> PassResource::Buffer(const std::string &name) const {
    if (read_buffers_.contains(name)) {
        auto handle = read_buffers_.at(name).handle;
        return graph_->Buffer(handle).buffer;
    } else {
        auto handle = write_buffers_.at(name).handle;
        return graph_->Buffer(handle).buffer;
    }
}

Ref<gfx::Texture> PassResource::Texture(const std::string &name) const {
    if (read_textures_.contains(name)) {
        auto handle = read_textures_.at(name).handle;
        return graph_->Texture(handle).texture;
    } else {
        auto handle = write_textures_.at(name).handle;
        return graph_->Texture(handle).texture;
    }
}

void PassResource::GetShaderBarriers(RenderGraph &rg, bool is_render_pass,
    Vec<gfx::BufferBarrier> &buffer_barriers, Vec<gfx::TextureBarrier> &texture_barriers) {
    for (auto &[_, handle] : read_buffers_) {
        auto target_access_type = handle.type == BufferReadType::eStorage
            ? (is_render_pass
                ? gfx::ResourceAccessType::eRenderShaderStorageResourceRead
                : gfx::ResourceAccessType::eComputeShaderStorageResourceRead)
            : (is_render_pass
                ? gfx::ResourceAccessType::eRenderShaderUniformBufferRead
                : gfx::ResourceAccessType::eComputeShaderUniformBufferRead);

        auto &buffer = rg.Buffer(handle.handle);
        if (target_access_type != buffer.access_type) {
            buffer_barriers.push_back(gfx::BufferBarrier {
                .buffer = buffer.buffer,
                .src_access_type = buffer.access_type,
                .dst_access_type = target_access_type,
            });
            buffer.access_type = target_access_type;
        }
    }
    auto target_access_type = is_render_pass
        ? gfx::ResourceAccessType::eRenderShaderSampledTextureRead
        : gfx::ResourceAccessType::eComputeShaderSampledTextureRead;
    for (auto &[_, handle] : read_textures_) {
        auto &texture = rg.Texture(handle.handle);
        if (target_access_type != texture.access_type) {
            texture_barriers.push_back(gfx::TextureBarrier {
                .texture = { texture.texture },
                .src_access_type = texture.access_type,
                .dst_access_type = target_access_type,
            });
            texture.access_type = target_access_type;
        }
    }

    target_access_type = is_render_pass
        ? gfx::ResourceAccessType::eRenderShaderStorageResourceWrite
        : gfx::ResourceAccessType::eComputeShaderStorageResourceWrite;
    for (auto &[_, handle] : write_buffers_) {
        auto &buffer = rg.Buffer(handle.handle);
        if (target_access_type != buffer.access_type) {
            buffer_barriers.push_back(gfx::BufferBarrier {
                .buffer = buffer.buffer,
                .src_access_type = buffer.access_type,
                .dst_access_type = target_access_type,
            });
            buffer.access_type = target_access_type;
        }
    }
    for (auto &[_, handle] : write_textures_) {
        auto &texture = rg.Texture(handle.handle);
        if (target_access_type != texture.access_type) {
            texture_barriers.push_back(gfx::TextureBarrier {
                .texture = { texture.texture },
                .src_access_type = texture.access_type,
                .dst_access_type = target_access_type,
            });
            texture.access_type = target_access_type;
        }
    }
}

RenderPassColorTargetBuilder::RenderPassColorTargetBuilder(TextureHandle handle) {
    target_.handle = handle;
}

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

RenderPassColorTargetBuilder &RenderPassColorTargetBuilder::GenerateMipmaps() {
    target_.generate_mipmaps = true;
    return *this;
}

RenderPassDepthStencilTargetBuilder::RenderPassDepthStencilTargetBuilder(TextureHandle handle) {
    target_.handle = handle;
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

RenderPassDepthStencilTargetBuilder &RenderPassDepthStencilTargetBuilder::GenerateMipmaps() {
    target_.generate_mipmaps = true;
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
    read_buffers_[name] = { handle, type };
    return *this;
}

RenderPassBuilder &RenderPassBuilder::Read(const std::string &name, TextureHandle handle) {
    read_textures_[name] = { handle };
    return *this;
}

RenderPassBuilder &RenderPassBuilder::Write(const std::string &name, BufferHandle handle) {
    write_buffers_[name] = { handle };
    return *this;
}

RenderPassBuilder &RenderPassBuilder::Write(const std::string &name, TextureHandle handle, bool generate_mipmaps) {
    write_textures_[name] = { handle, generate_mipmaps };
    return *this;
}

ComputePassBuilder &ComputePassBuilder::Read(const std::string &name, BufferHandle handle, BufferReadType type) {
    read_buffers_[name] = { handle, type };
    return *this;
}

ComputePassBuilder &ComputePassBuilder::Read(const std::string &name, TextureHandle handle) {
    read_textures_[name] = { handle };
    return *this;
}

ComputePassBuilder &ComputePassBuilder::Write(const std::string &name, BufferHandle handle) {
    write_buffers_[name] = { handle };
    return *this;
}

ComputePassBuilder &ComputePassBuilder::Write(const std::string &name, TextureHandle handle, bool generate_mipmaps) {
    write_textures_[name] = { handle, generate_mipmaps };
    return *this;
}

BlitPassBuilder &BlitPassBuilder::Src(TextureHandle handle, uint32_t level, uint32_t layer) {
    src_handle_ = handle;
    src_level_ = level;
    src_layer_ = layer;
    return *this;
}

BlitPassBuilder &BlitPassBuilder::Dst(TextureHandle handle, uint32_t level, uint32_t layer) {
    dst_handle_ = handle;
    dst_level_ = level;
    dst_layer_ = layer;
    return *this;
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
