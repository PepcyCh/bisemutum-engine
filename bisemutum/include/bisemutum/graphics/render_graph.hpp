#pragma once

#include <any>
#include <functional>

#include "handles.hpp"
#include "resource.hpp"
#include "resource_builder.hpp"
#include "accel.hpp"
#include "render_graph_pass.hpp"
#include "rendered_object_list.hpp"
#include "../prelude/idiom.hpp"
#include "../rhi/device.hpp"

namespace bi::gfx {

struct GraphicsManager;

struct GraphicsPassBuilder;
struct ComputePassBuilder;

struct RenderGraph final : PImpl<RenderGraph> {
    struct Impl;

    RenderGraph();
    ~RenderGraph();

    auto add_buffer(std::function<auto(BufferBuilder&) -> void> setup_func) -> BufferHandle;
    auto import_buffer(Ref<Buffer> buffer) -> BufferHandle;
    auto add_texture(std::function<auto(TextureBuilder&) -> void> setup_func) -> TextureHandle;
    auto import_texture(
        Ref<Texture> texture, BitFlags<rhi::ResourceAccessType> access = rhi::ResourceAccessType::none
    ) -> TextureHandle;
    auto import_back_buffer() const -> TextureHandle;

    auto add_acceleration_structure(AccelerationStructureDesc const& desc) -> AccelerationStructureHandle;

    template <typename PassData>
    auto add_graphics_pass(std::string_view name) -> std::pair<GraphicsPassBuilder&, Ref<PassData>> {
        std::any pass_data_any = PassData{};
        auto const& [builder, p_pass_data_any] = add_graphics_pass_impl(name, std::move(pass_data_any));
        auto* pass_data = std::any_cast<PassData>(p_pass_data_any);
        return {builder, unsafe_make_ref(pass_data)};
    }

    template <typename PassData>
    auto add_compute_pass(std::string_view name) -> std::pair<ComputePassBuilder&, Ref<PassData>> {
        std::any pass_data_any = PassData{};
        auto const& [builder, p_pass_data_any] = add_compute_pass_impl(name, std::move(pass_data_any));
        auto* pass_data = std::any_cast<PassData>(p_pass_data_any);
        return {builder, unsafe_make_ref(pass_data)};
    }

    template <typename PassData>
    auto add_raytracing_pass(std::string_view name) -> std::pair<RaytracingPassBuilder&, Ref<PassData>> {
        std::any pass_data_any = PassData{};
        auto const& [builder, p_pass_data_any] = add_raytracing_pass_impl(name, std::move(pass_data_any));
        auto* pass_data = std::any_cast<PassData>(p_pass_data_any);
        return {builder, unsafe_make_ref(pass_data)};
    }

    enum class BlitPassMode : uint8_t {
        normal,
        equitangular_to_cubemap,
    };
    auto add_blit_pass(
        std::string_view name,
        TextureHandle src, uint32_t src_mip_level, uint32_t src_array_layer,
        TextureHandle dst, uint32_t dst_mip_level, uint32_t dst_array_layer,
        BlitPassMode mode = BlitPassMode::normal
    ) -> void;
    auto add_blit_pass(
        std::string_view name, TextureHandle src, TextureHandle dst, BlitPassMode mode = BlitPassMode::normal
    ) -> void {
        add_blit_pass(name, src, 0, 0, dst, 0, 0, mode);
    }

    auto add_rendered_object_list(RenderedObjectListDesc const& desc) -> RenderedObjectListHandle;

    auto execute() -> void;

    auto buffer(BufferHandle handle) const -> Ref<Buffer>;
    auto texture(TextureHandle handle) const -> Ref<Texture>;

    auto take_buffer(BufferHandle handle) -> Box<Buffer>;
    auto take_texture(TextureHandle handle) -> Box<Texture>;

    auto acceleration_structure(AccelerationStructureHandle handle) const -> Ref<AccelerationStructure>;

    auto rendered_object_list(RenderedObjectListHandle handle) const -> CRef<RenderedObjectList>;

private:
    auto add_graphics_pass_impl(
        std::string_view name, std::any&& pass_data
    ) -> std::pair<GraphicsPassBuilder&, std::any*>;

    auto add_compute_pass_impl(
        std::string_view name, std::any&& pass_data
    ) -> std::pair<ComputePassBuilder&, std::any*>;

    auto add_raytracing_pass_impl(
        std::string_view name, std::any&& pass_data
    ) -> std::pair<RaytracingPassBuilder&, std::any*>;

    friend GraphicsManager;
    auto set_graphics_device(Ref<rhi::Device> device, uint32_t num_frames) -> void;
    auto set_back_buffer(Ref<Texture> texture, BitFlags<rhi::ResourceAccessType> access) -> void;
    auto set_command_encoder(Ref<rhi::CommandEncoder> cmd_encoder) -> void;

    friend GraphicsPassBuilder;
    friend ComputePassBuilder;
    friend RaytracingPassBuilder;
    auto add_read_edge(size_t pass_index, BufferHandle handle) -> BufferHandle;
    auto add_read_edge(size_t pass_index, TextureHandle handle) -> TextureHandle;
    auto add_read_edge(size_t pass_index, AccelerationStructureHandle handle) -> AccelerationStructureHandle;
    auto add_write_edge(size_t pass_index, BufferHandle handle) -> BufferHandle;
    auto add_write_edge(size_t pass_index, TextureHandle handle) -> TextureHandle;
};

}
