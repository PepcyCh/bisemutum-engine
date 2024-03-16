#include <bisemutum/graphics/render_graph_pass.hpp>

#include <bisemutum/graphics/render_graph.hpp>

namespace bi::gfx {

GraphicsPassColorTargetBuilder::GraphicsPassColorTargetBuilder(TextureHandle handle) {
    target_.handle = handle;
}
auto GraphicsPassColorTargetBuilder::array_layer(
    uint32_t base_layer, uint32_t num_layers
) -> GraphicsPassColorTargetBuilder& {
    target_.base_layer = base_layer;
    target_.num_layers = num_layers;
    return *this;
}
auto GraphicsPassColorTargetBuilder::mip_level(uint32_t level) -> GraphicsPassColorTargetBuilder& {
    target_.level = level;
    return *this;
}
auto GraphicsPassColorTargetBuilder::clear_color(float4 const& color) -> GraphicsPassColorTargetBuilder& {
    target_.clear_color = color;
    return *this;
}
auto GraphicsPassColorTargetBuilder::dont_store() -> GraphicsPassColorTargetBuilder& {
    target_.store = false;
    return *this;
}
auto GraphicsPassColorTargetBuilder::generate_mipmaps() -> GraphicsPassColorTargetBuilder& {
    target_.generate_mipmaps = true;
    return *this;
}

GraphicsPassDepthStencilTargetBuilder::GraphicsPassDepthStencilTargetBuilder(TextureHandle handle) {
    target_.handle = handle;
}
auto GraphicsPassDepthStencilTargetBuilder::array_layer(
    uint32_t base_layer, uint32_t num_layers
) -> GraphicsPassDepthStencilTargetBuilder& {
    target_.base_layer = base_layer;
    target_.num_layers = num_layers;
    return *this;
}
auto GraphicsPassDepthStencilTargetBuilder::mip_level(uint32_t level) -> GraphicsPassDepthStencilTargetBuilder& {
    target_.level = level;
    return *this;
}
auto GraphicsPassDepthStencilTargetBuilder::clear_depth_stencil(
    float depth, uint8_t stencil
) -> GraphicsPassDepthStencilTargetBuilder& {
    target_.clear_value = std::make_pair(depth, stencil);
    return *this;
}
auto GraphicsPassDepthStencilTargetBuilder::dont_store() -> GraphicsPassDepthStencilTargetBuilder& {
    target_.store = false;
    return *this;
}
auto GraphicsPassDepthStencilTargetBuilder::generate_mipmaps() -> GraphicsPassDepthStencilTargetBuilder& {
    target_.generate_mipmaps = true;
    return *this;
}
auto GraphicsPassDepthStencilTargetBuilder::read_only() -> GraphicsPassDepthStencilTargetBuilder& {
    target_.read_only = true;
    return *this;
}

auto GraphicsPassBuilder::use_color(uint32_t index, GraphicsPassColorTargetBuilder const& target) -> TextureHandle {
    if (index < color_targets_.size()) {
        color_targets_[index] = target;
        auto handle = color_targets_[index].value().handle;
        handle = rg_->add_write_edge(pass_index_, handle);
        return handle;
    } else {
        return static_cast<TextureHandle>(-1);
    }
}
auto GraphicsPassBuilder::use_depth_stencil(GraphicsPassDepthStencilTargetBuilder const& target) -> TextureHandle {
    depth_stencil_target_ = target;
    auto handle = depth_stencil_target_.value().handle;
    if (!depth_stencil_target_.value().read_only) {
        handle = rg_->add_write_edge(pass_index_, handle);
    } else {
        handle = rg_->add_read_edge(pass_index_, handle);
    }
    return handle;
}
auto GraphicsPassBuilder::read(BufferHandle handle) -> BufferHandle {
    read_buffers_.push_back(handle);
    handle = rg_->add_read_edge(pass_index_, handle);
    return handle;
}
auto GraphicsPassBuilder::read(TextureHandle handle) -> TextureHandle {
    read_textures_.push_back(handle);
    handle = rg_->add_read_edge(pass_index_, handle);
    return handle;
}
auto GraphicsPassBuilder::read(AccelerationStructureHandle handle) -> AccelerationStructureHandle {
    handle = rg_->add_read_edge(pass_index_, handle);
    return handle;
}
auto GraphicsPassBuilder::write(BufferHandle handle) -> BufferHandle {
    write_buffers_.push_back(handle);
    handle = rg_->add_write_edge(pass_index_, handle);
    return handle;
}
auto GraphicsPassBuilder::write(TextureHandle handle) -> TextureHandle {
    write_textures_.push_back(handle);
    handle = rg_->add_write_edge(pass_index_, handle);
    return handle;
}
auto GraphicsPassBuilder::set_execution_function_impl(
    std::function<auto(std::any const*, GraphicsPassContext const&) -> void> func
) -> void {
    execution_func_ = std::move(func);
}

auto ComputePassBuilder::read(BufferHandle handle) -> BufferHandle {
    read_buffers_.push_back(handle);
    handle = rg_->add_read_edge(pass_index_, handle);
    return handle;
}
auto ComputePassBuilder::read(TextureHandle handle) -> TextureHandle {
    read_textures_.push_back(handle);
    handle = rg_->add_read_edge(pass_index_, handle);
    return handle;
}
auto ComputePassBuilder::read(AccelerationStructureHandle handle) -> AccelerationStructureHandle {
    handle = rg_->add_read_edge(pass_index_, handle);
    return handle;
}
auto ComputePassBuilder::write(BufferHandle handle) -> BufferHandle {
    write_buffers_.push_back(handle);
    handle = rg_->add_write_edge(pass_index_, handle);
    return handle;
}
auto ComputePassBuilder::write(TextureHandle handle) -> TextureHandle {
    write_textures_.push_back(handle);
    handle = rg_->add_write_edge(pass_index_, handle);
    return handle;
}
auto ComputePassBuilder::set_execution_function_impl(
    std::function<auto(std::any const*, ComputePassContext const&) -> void> func
) -> void {
    execution_func_ = std::move(func);
}

}
