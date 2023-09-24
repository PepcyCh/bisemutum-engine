#pragma once

#include <functional>

#include "handles.hpp"
#include "renderer.hpp"
#include "displayer.hpp"
#include "mipmap_mode.hpp"
#include "../prelude/idiom.hpp"
#include "../rhi/pipeline.hpp"
#include "../utils/reflection.hpp"

namespace bi::rhi {

struct Device;

}

namespace bi::gfx {

struct GraphicsSettings final {
    rhi::Backend backend = rhi::Backend::vulkan;
    bool enable_validation = false;
    uint8_t num_swapchain_textures = 3;
    bool swapchain_srgb = true;
};
BI_SREFL(
    type(GraphicsSettings),
    field(backend),
    field(enable_validation),
    field(num_swapchain_textures),
    field(swapchain_srgb)
)

struct Buffer;
struct Texture;
struct Sampler;

struct RenderGraph;
struct ResourceBindingContext;
struct GraphicsPassContext;
struct ComputePassContext;
struct CommandHelpers;

struct FragmentShader;
struct ComputeShader;

struct Camera;
struct Drawable;

struct GraphicsManager final : PImpl<GraphicsManager> {
    struct Impl;

    GraphicsManager();
    ~GraphicsManager();

    auto initialize(GraphicsSettings const& settings) -> void;

    template <typename Renderer>
    auto register_renderer() -> void {
        register_renderer(std::string{Renderer::renderer_type_name}, []() -> Dyn<IRenderer>::Box { return Renderer{}; });
    }
    auto set_renderer(std::string_view renderer_type_name) -> void;

    template <typename Displayer>
    auto set_displayer() -> void {
        set_displayer(Displayer{});
    }
    auto set_displayer(Dyn<IDisplayer>::Ref displayer) -> void;

    auto add_camera() -> CameraHandle;
    auto remove_camera(CameraHandle handle) -> void;
    auto get_camera(CameraHandle handle) -> Ref<Camera>;
    auto for_each_camera(std::function<auto(Camera&) -> void> func) -> void;
    auto for_each_camera(std::function<auto(Camera const&) -> void> func) const -> void;

    auto add_drawable() -> DrawableHandle;
    auto remove_drawable(DrawableHandle handle) -> void;
    auto get_drawable(DrawableHandle handle) -> Ref<Drawable>;
    auto for_each_drawable(std::function<auto(Drawable&) -> void> func) -> void;
    auto for_each_drawable(std::function<auto(Drawable const&) -> void> func) const -> void;

    auto get_sampler(rhi::SamplerDesc const& desc) -> Ref<Sampler>;

    auto render_frame() -> void;

    auto execute_in_this_frame(std::function<auto(Ref<rhi::CommandEncoder>) -> void> func) -> void;
    auto execute_immediately(std::function<auto(Ref<rhi::CommandEncoder>) -> void> func) -> void;

    auto blit_texture_2d(
        Ref<rhi::CommandEncoder> cmd_encoder,
        Ref<Texture> src, uint32_t src_mip_level, uint32_t src_array_layer,
        Ref<Texture> dst, uint32_t dst_mip_level, uint32_t dst_array_layer
    ) -> void;
    auto blit_texture_2d(Ref<rhi::CommandEncoder> cmd_encoder, Ref<Texture> src, Ref<Texture> dst) -> void {
        blit_texture_2d(cmd_encoder, src, 0, 0, dst, 0, 0);
    }

    auto generate_mipmaps_2d(
        Ref<rhi::CommandEncoder> cmd_encoder,
        Ref<Texture> texture,
        BitFlags<rhi::ResourceAccessType>& texture_access,
        MipmapMode mode = MipmapMode::average
    ) -> void;

    auto device() -> Ref<rhi::Device>;
    auto render_graph() -> RenderGraph&;

    auto num_frames_in_flight() const -> uint32_t;

private:
    auto register_renderer(std::string&& name, std::function<auto() -> Dyn<IRenderer>::Box> creator) -> void;

    friend Buffer;
    friend Texture;
    friend Sampler;
    auto curr_frame_index() const -> uint32_t;
    auto cpu_resource_descriptor_heap() -> Ref<rhi::DescriptorHeap>;
    auto cpu_sampler_descriptor_heap() -> Ref<rhi::DescriptorHeap>;

    friend GraphicsPassContext;
    auto compile_pipeline_for_drawable(
        GraphicsPassContext const* graphics_context, CRef<Camera> camera, Ref<Drawable> drawable, CRef<FragmentShader> fs
    ) -> Ref<rhi::GraphicsPipeline>;

    friend ResourceBindingContext;
    friend CommandHelpers;
    auto get_descriptors_for(
        std::vector<rhi::DescriptorHandle> cpu_descriptors,
        std::vector<rhi::DescriptorType> const& desc_types,
        rhi::BindGroupLayout const& layout
    ) -> rhi::DescriptorHandle;
};

}