#pragma once

#include <functional>

#include "renderer.hpp"
#include "displayer.hpp"
#include "mipmap_mode.hpp"
#include "../prelude/idiom.hpp"
#include "../prelude/move_only_function.hpp"
#include "../rhi/pipeline.hpp"
#include "../utils/srefl.hpp"

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

struct ShaderCompiler;
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

    auto wait_idle() -> void;

    auto new_frame() -> void;

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
    auto shader_compiler() -> Ref<ShaderCompiler>;
    auto render_graph() -> RenderGraph&;

    auto swapchain_format() const -> rhi::ResourceFormat;
    auto num_frames_in_flight() const -> uint32_t;
    auto curr_frame_index() const -> uint32_t;

    auto get_gpu_descriptor_for(
        std::vector<rhi::DescriptorHandle> cpu_descriptors,
        rhi::BindGroupLayout const& layout
    ) -> rhi::DescriptorHandle;

    auto add_delayed_destroy(MoveOnlyFunction<auto() -> void> destroy) -> void;

private:
    auto register_renderer(std::string&& name, std::function<auto() -> Dyn<IRenderer>::Box> creator) -> void;

    friend Buffer;
    friend Texture;
    friend Sampler;
    auto allocate_cpu_descriptor(rhi::DescriptorType type) -> rhi::DescriptorHandle;
    auto free_cpu_resource_descriptor(rhi::DescriptorHandle descriptor) -> void;
    auto free_cpu_sampler_descriptor(rhi::DescriptorHandle descriptor) -> void;

    friend GraphicsPassContext;
    auto compile_pipeline_for_drawable(
        GraphicsPassContext const* graphics_context, CRef<Camera> camera, Ref<Drawable> drawable, CRef<FragmentShader> fs
    ) -> Ref<rhi::GraphicsPipeline>;
};

}
