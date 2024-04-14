#pragma once

#include <functional>

#include "buffer_suballocator.hpp"
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
struct AccelerationStructure;
struct GeometryAccelerationStructure;
struct Sampler;

struct ShaderCompiler;
struct RenderGraph;
struct GraphicsPassContext;
struct ComputePassContext;
struct RaytracingPassContext;
struct CommandHelpers;

struct FragmentShader;
struct ComputeShader;
struct RaytracingShaders;

struct Camera;
struct Material;
struct Drawable;
struct MeshData;

struct GpuSceneData;
struct GpuSceneSystem;

enum class DefaultTexture : uint8_t {
    white_1x1,
    black_1x1,
    normal_1x1,
    black_1x1_cube,

    last_ = black_1x1_cube,
};
inline constexpr auto num_default_textures = static_cast<size_t>(DefaultTexture::last_) + 1;

struct GraphicsManager final : PImpl<GraphicsManager> {
    struct Impl;

    GraphicsManager();
    ~GraphicsManager();

    auto initialize(GraphicsSettings const& settings, std::string_view pipeline_cache_file) -> void;

    auto wait_idle() -> void;

    auto new_frame() -> void;

    template <typename Renderer>
    auto register_renderer() -> void {
        register_renderer(std::string{Renderer::renderer_type_name}, []() -> Dyn<IRenderer>::Box { return Renderer{}; });
    }
    auto set_renderer(std::string_view renderer_type_name) -> void;
    auto get_renderer() const -> Dyn<IRenderer>::CRef;

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

    auto equitangular_to_cubemap(
        Ref<rhi::CommandEncoder> cmd_encoder,
        Ref<Texture> src, uint32_t src_mip_level, uint32_t src_array_layer,
        Ref<Texture> dst, uint32_t dst_mip_level, uint32_t dst_array_layer
    ) -> void;
    auto equitangular_to_cubemap(Ref<rhi::CommandEncoder> cmd_encoder, Ref<Texture> src, Ref<Texture> dst) -> void {
        equitangular_to_cubemap(cmd_encoder, src, 0, 0, dst, 0, 0);
    }

    auto device() -> Ref<rhi::Device>;
    auto shader_compiler() -> Ref<ShaderCompiler>;
    auto render_graph() -> RenderGraph&;

    auto default_buffer() -> Ref<Buffer>;
    // Default textures are some special fallback textures that can be used as a default value for material, skybox...
    auto default_texture(DefaultTexture index) -> Ref<Texture>;
    // Dummy textures are used for resource binding when a nullptr is passed as shader param.
    auto dummy_texture(rhi::ResourceFormat format, rhi::TextureViewType type) -> Ref<Texture>;

    auto swapchain_format() const -> rhi::ResourceFormat;
    auto num_frames_in_flight() const -> uint32_t;
    auto curr_frame_index() const -> uint32_t;

    auto get_gpu_descriptor_for(
        std::vector<rhi::DescriptorHandle> const& cpu_descriptors,
        rhi::BindGroupLayout const& layout
    ) -> rhi::DescriptorHandle;

    auto add_delayed_destroy(MoveOnlyFunction<auto() -> void> destroy) -> void;

private:
    auto register_renderer(std::string&& name, std::function<auto() -> Dyn<IRenderer>::Box> creator) -> void;

    friend Buffer;
    friend Texture;
    friend Sampler;
    friend AccelerationStructure;
    auto allocate_cpu_descriptor(rhi::DescriptorType type) -> rhi::DescriptorHandle;
    auto free_cpu_resource_descriptor(rhi::DescriptorHandle descriptor) -> void;
    auto free_cpu_sampler_descriptor(rhi::DescriptorHandle descriptor) -> void;

    friend Material;
    auto get_material_params_buffers() -> Ref<BufferSuballocator>;
    auto add_material_texture(Ref<Texture> texture) -> size_t;
    auto remove_material_texture(size_t index) -> void;
    auto add_material_sampler(Ref<Sampler> sampler) -> size_t;
    auto remove_material_sampler(size_t index) -> void;

    friend GpuSceneSystem;
    auto fill_gpu_scene_data(Ref<GpuSceneData> gpu_scene_data) -> void;

    friend RenderGraph;
    auto update_mesh_buffers(CRef<MeshData> mesh) -> void;
    auto require_blas_build_desc(CRef<Drawable> drawable)
        -> std::pair<Option<rhi::AccelerationStructureGeometryBuildInput>, Ref<GeometryAccelerationStructure>>;

    friend GraphicsPassContext;
    auto bind_mesh_buffers(
        Ref<rhi::GraphicsCommandEncoder> cmd_encoder, CRef<MeshData> mesh
    ) -> void;
    auto draw_drawable(
        Ref<rhi::GraphicsCommandEncoder> cmd_encoder, Ref<Drawable> drawable
    ) -> void;
    auto compile_pipeline_for_drawable(
        GraphicsPassContext const* graphics_context, CRef<Camera> camera, Ref<Drawable> drawable, CRef<FragmentShader> fs
    ) -> Ref<rhi::GraphicsPipeline>;

    friend ComputePassContext;
    auto compile_pipeline_compute(CPtr<Camera> camera, CRef<ComputeShader> cs) -> Ref<rhi::ComputePipeline>;

    friend RaytracingPassContext;
    auto compile_pipeline_raytracing(
        RaytracingPassContext const* rt_context, CRef<Camera> camera, CRef<RaytracingShaders> shaders
    ) -> std::pair<Ref<rhi::RaytracingPipeline>, rhi::RaytracingShaderBindingTableBuffers>;
};

}
