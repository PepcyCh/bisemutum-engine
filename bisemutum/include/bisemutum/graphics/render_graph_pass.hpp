#pragma once

#include <any>
#include <array>
#include <functional>

#include "handles.hpp"
#include "render_graph_context.hpp"
#include "mipmap_mode.hpp"
#include "../rhi/command.hpp"
#include "../math/math.hpp"
#include "../prelude/ref.hpp"

namespace bi::gfx {

struct RenderGraph;

enum class BufferReadType : uint8_t {
    uniform,
    storage,
    indirect_args,
};
struct PassReadBuffer final {
    BufferHandle handle;
    BufferReadType type = BufferReadType::uniform;
};
struct PassWriteBuffer final {
    BufferHandle handle;
};
struct PassReadTexture final {
    TextureHandle handle;
};
struct PassWriteTexture final {
    TextureHandle handle;
    bool generate_mipmaps = false;
};

struct GraphicsPassColorTarget {
    TextureHandle handle;
    uint32_t base_layer = 0;
    uint32_t num_layers = 1;
    uint32_t level = 0;
    Option<float4> clear_color = {};
    bool store = true;
    Option<MipmapMode> mipmap_mode = {};
};
struct GraphicsPassColorTargetBuilder final {
    GraphicsPassColorTargetBuilder(TextureHandle handle);

    operator GraphicsPassColorTarget() const { return target_; }

    auto array_layer(uint32_t base_layer, uint32_t num_layers = 1) -> GraphicsPassColorTargetBuilder&;
    auto mip_level(uint32_t level) -> GraphicsPassColorTargetBuilder&;
    auto clear_color(float4 const& color = {0.0f, 0.0f, 0.0f, 1.0f}) -> GraphicsPassColorTargetBuilder&;
    auto dont_store() -> GraphicsPassColorTargetBuilder&;
    auto generate_mipmaps(MipmapMode mode = MipmapMode::average) -> GraphicsPassColorTargetBuilder&;

private:
    GraphicsPassColorTarget target_;
};

struct GraphicsPassDepthStencilTarget final {
    TextureHandle handle;
    uint32_t base_layer = 0;
    uint32_t num_layers = 1;
    uint32_t level = 0;
    Option<std::pair<float, uint8_t>> clear_value = {};
    bool store = true;
    Option<MipmapMode> mipmap_mode = {};
    bool read_only = false;
};
struct GraphicsPassDepthStencilTargetBuilder final {
    GraphicsPassDepthStencilTargetBuilder(TextureHandle handle);

    operator GraphicsPassDepthStencilTarget() const { return target_; }

    auto array_layer(uint32_t base_layer, uint32_t num_layers = 1) -> GraphicsPassDepthStencilTargetBuilder&;
    auto mip_level(uint32_t level) -> GraphicsPassDepthStencilTargetBuilder&;
    auto clear_depth_stencil(float depth = 0.0f, uint8_t stencil = 0) -> GraphicsPassDepthStencilTargetBuilder&;
    auto dont_store() -> GraphicsPassDepthStencilTargetBuilder&;
    auto generate_mipmaps(MipmapMode mode = MipmapMode::average) -> GraphicsPassDepthStencilTargetBuilder&;
    auto read_only() -> GraphicsPassDepthStencilTargetBuilder&;

private:
    GraphicsPassDepthStencilTarget target_;
};

struct GraphicsPassBuilder final {
    auto use_color(uint32_t index, GraphicsPassColorTargetBuilder const& target) -> TextureHandle;
    auto use_depth_stencil(GraphicsPassDepthStencilTargetBuilder const& target) -> TextureHandle;

    auto read(BufferHandle handle) -> BufferHandle;
    auto read(TextureHandle handle) -> TextureHandle;
    auto read(AccelerationStructureHandle handle) -> AccelerationStructureHandle;

    auto write(BufferHandle handle) -> BufferHandle;
    auto write(TextureHandle handle) -> TextureHandle;

    template <typename PassData>
    auto set_execution_function(
        std::function<auto(CRef<PassData>, GraphicsPassContext const&) -> void> func
    ) -> void {
        set_execution_function_impl(
            [func = std::move(func)](std::any const* pass_data, GraphicsPassContext const& ctx) {
                func(unsafe_make_cref(std::any_cast<PassData>(pass_data)), ctx);
            }
        );
    }

private:
    auto set_execution_function_impl(
        std::function<auto(std::any const*, GraphicsPassContext const&) -> void> func
    ) -> void;

    friend RenderGraph;

    RenderGraph* rg_;
    size_t pass_index_;

    std::array<Option<GraphicsPassColorTarget>, rhi::max_num_render_targets> color_targets_;
    Option<GraphicsPassDepthStencilTarget> depth_stencil_target_;

    std::vector<BufferHandle> read_buffers_;
    std::vector<BufferHandle> write_buffers_;
    std::vector<TextureHandle> read_textures_;
    std::vector<TextureHandle> write_textures_;

    std::function<auto(std::any const*, GraphicsPassContext const&) -> void> execution_func_;
};

struct ComputePassBuilder final {
    auto read(BufferHandle handle) -> BufferHandle;
    auto read(TextureHandle handle) -> TextureHandle;
    auto read(AccelerationStructureHandle handle) -> AccelerationStructureHandle;

    auto write(BufferHandle handle) -> BufferHandle;
    auto write(TextureHandle handle) -> TextureHandle;

    template <typename PassData>
    auto set_execution_function(
        std::function<auto(CRef<PassData>, ComputePassContext const&) -> void> func
    ) -> void {
        set_execution_function_impl(
            [func = std::move(func)](std::any const* pass_data, ComputePassContext const& ctx) {
                func(unsafe_make_cref(std::any_cast<PassData>(pass_data)), ctx);
            }
        );
    }

private:
    auto set_execution_function_impl(
        std::function<auto(std::any const*, ComputePassContext const&) -> void> func
    ) -> void;

    friend RenderGraph;

    RenderGraph* rg_;
    size_t pass_index_;

    std::vector<BufferHandle> read_buffers_;
    std::vector<BufferHandle> write_buffers_;
    std::vector<TextureHandle> read_textures_;
    std::vector<TextureHandle> write_textures_;

    std::function<auto(std::any const*, ComputePassContext const&) -> void> execution_func_;
};

struct RaytracingPassBuilder final {
    auto read(BufferHandle handle) -> BufferHandle;
    auto read(TextureHandle handle) -> TextureHandle;
    auto read(AccelerationStructureHandle handle) -> AccelerationStructureHandle;

    auto write(BufferHandle handle) -> BufferHandle;
    auto write(TextureHandle handle) -> TextureHandle;

    template <typename PassData>
    auto set_execution_function(
        std::function<auto(CRef<PassData>, RaytracingPassContext const&) -> void> func
    ) -> void {
        set_execution_function_impl(
            [func = std::move(func)](std::any const* pass_data, RaytracingPassContext const& ctx) {
                func(unsafe_make_cref(std::any_cast<PassData>(pass_data)), ctx);
            }
        );
    }

private:
    auto set_execution_function_impl(
        std::function<auto(std::any const*, RaytracingPassContext const&) -> void> func
    ) -> void;

    friend RenderGraph;

    RenderGraph* rg_;
    size_t pass_index_;

    std::vector<BufferHandle> read_buffers_;
    std::vector<BufferHandle> write_buffers_;
    std::vector<TextureHandle> read_textures_;
    std::vector<TextureHandle> write_textures_;

    std::function<auto(std::any const*, RaytracingPassContext const&) -> void> execution_func_;
};

}
