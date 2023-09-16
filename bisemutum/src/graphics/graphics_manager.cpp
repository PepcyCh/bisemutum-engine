#include <bisemutum/graphics/graphics_manager.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/logger.hpp>
#include <bisemutum/window/window.hpp>
#include <bisemutum/rhi/device.hpp>
#include <bisemutum/graphics/shader_compiler.hpp>
#include <bisemutum/graphics/render_graph.hpp>
#include <bisemutum/graphics/shader.hpp>
#include <bisemutum/graphics/camera.hpp>
#include <bisemutum/graphics/drawable.hpp>
#include <bisemutum/containers/slotmap.hpp>
#include <bisemutum/prelude/math.hpp>
#include <fmt/format.h>

#include "descriptor_sets.hpp"
#include "command_helpers.hpp"

template <>
struct std::hash<bi::rhi::DescriptorHandle> final {
    auto operator()(bi::rhi::DescriptorHandle const& v) const noexcept -> size_t {
        return bi::hash(v.cpu, v.gpu);
    }
};

namespace bi::gfx {

namespace {

constexpr uint32_t cpu_resource_desc_heap_size = 8192;
constexpr uint32_t gpu_resource_desc_heap_size = 2048;
constexpr uint32_t cpu_sampler_desc_heap_size = 1024;
constexpr uint32_t gpu_sampler_desc_heap_size = 1024;

}

struct GraphicsManager::Impl final {
    auto initialize(GraphicsSettings const& settings) -> void {
        device = rhi::Device::create(rhi::DeviceDesc{
            .backend = settings.backend,
            .enable_validation = settings.enable_validation,
        });
        graphics_queue = device->get_queue(rhi::QueueType::graphics);
        compute_queue = device->get_queue(rhi::QueueType::compute);

        shader_compiler.initialize(device.ref());
        command_helpers.initialize(device.ref(), shader_compiler);

        auto window = g_engine->window();
        swapchain = device->create_swapchain(rhi::SwapchainDesc{
            .queue = graphics_queue.value(),
            .width = window->width(),
            .height = window->height(),
            .window_handle = window->platform_handle(),
        });
        swapchain_resize_callback = window->register_resize_callback([this](uint32_t width, uint32_t height) {
            immediate_execution_fence->signal_on(graphics_queue.value());
            immediate_execution_fence->wait();
            swapchain->resize(width, height);
        });

        cpu_resource_heap = device->create_descriptor_heap(rhi::DescriptorHeapDesc{
            .max_count = cpu_resource_desc_heap_size,
            .type = rhi::DescriptorHeapType::resource,
            .shader_visible = false,
        });
        cpu_sampler_heap = device->create_descriptor_heap(rhi::DescriptorHeapDesc{
            .max_count = cpu_sampler_desc_heap_size,
            .type = rhi::DescriptorHeapType::sampler,
            .shader_visible = false,
        });

        rhi::CommandPoolDesc cmd_pool_desc{
            .queue = graphics_queue.value(),
        };
        rhi::DescriptorHeapDesc gpu_resource_heap_desc{
            .max_count = gpu_resource_desc_heap_size,
            .type = rhi::DescriptorHeapType::resource,
            .shader_visible = true,
        };
        rhi::DescriptorHeapDesc gpu_sampler_heap_desc{
            .max_count = gpu_sampler_desc_heap_size,
            .type = rhi::DescriptorHeapType::sampler,
            .shader_visible = true,
        };
        frame_data.resize(settings.num_swapchain_textures);
        for (auto& fd : frame_data) {
            fd.acquire_semaphore = device->create_semaphore();
            fd.signal_semaphore = device->create_semaphore();
            fd.fence = device->create_fence();
            fd.graphics_cmd_pool = device->create_command_pool(cmd_pool_desc);
            fd.resource_heap = device->create_descriptor_heap(gpu_resource_heap_desc);
            fd.sampler_heap = device->create_descriptor_heap(gpu_sampler_heap_desc);
        }
        immediate_execution_fence = device->create_fence();

        render_graph.set_graphics_device(device.ref(), frame_data.size());
    }

    auto set_renderer(Dyn<IRenderer>::Box&& renderer) -> void {
        this->renderer = std::move(renderer);
    }

    auto set_displayer(Dyn<IDisplayer>::Ref displayer) -> void {
        this->displayer = &displayer;
    }

    auto add_camera() -> CameraHandle {
        return cameras.emplace();
    }
    auto remove_camera(CameraHandle handle) -> void {
        cameras.remove(handle);
    }
    auto get_camera(CameraHandle handle) -> Ref<Camera> {
        return cameras.get(handle);
    }
    auto for_each_camera(std::function<auto(Camera&) -> void>&& func) -> void {
        for (auto& camera : cameras) {
            func(camera);
        }
    }
    auto for_each_camera(std::function<auto(Camera const&) -> void>&& func) const -> void {
        for (auto const& camera : cameras) {
            func(camera);
        }
    }

    auto add_drawable() -> DrawableHandle {
        return drawables.emplace();
    }
    auto remove_drawable(DrawableHandle handle) -> void {
        drawables.remove(handle);
    }
    auto get_drawable(DrawableHandle handle) -> Ref<Drawable> {
        return drawables.get(handle);
    }
    auto for_each_drawable(std::function<auto(Drawable&) -> void> func) -> void {
        for (auto& drawable : drawables) {
            func(drawable);
        }
    }
    auto for_each_drawable(std::function<auto(Drawable const&) -> void> func) const -> void {
        for (auto const& drawable : drawables) {
            func(drawable);
        }
    }

    auto get_sampler(rhi::SamplerDesc const& desc) -> Ref<Sampler> {
        if (auto it = samplers.find(desc); it != samplers.end()) {
            return it->second;
        }
        auto& sampler = samplers.insert({desc, Sampler{}}).first->second;
        sampler.initialize(desc);
        return sampler;
    }

    auto render_frame() -> void {
        if (!renderer.has_value() || !displayer.has_value()) { return; }

        auto& fd = curr_frame_data();
        swapchain->acquire_next_texture(fd.acquire_semaphore.ref());
        fd.fence->wait();
        if (fd.camera_semaphores.size() < cameras.size()) {
            for (size_t i = fd.camera_semaphores.size(); i < cameras.size(); i++) {
                fd.camera_semaphores.push_back(device->create_semaphore());
            }
        }
        fd.graphics_cmd_pool->reset();

        renderer.prepare_renderer_per_frame_data();

        // Render each camera
        for (size_t camera_index = 0; auto& camera : cameras) {
            auto cmd_encoder = fd.graphics_cmd_pool->get_command_encoder();
            cmd_encoder->set_descriptor_heaps({
                fd.resource_heap.ref(),
                fd.sampler_heap.ref(),
            });
            render_graph.set_command_encoder(cmd_encoder.ref());
            render_graph.set_back_buffer(camera.target_texture());

            camera.update_shader_params();

            renderer.prepare_renderer_per_camera_data(camera);
            renderer.render_camera(camera);
            render_graph.execute();

            graphics_queue->submit_command_buffer(
                {cmd_encoder->finish()},
                {},
                {fd.camera_semaphores[camera_index].ref()}
            );

            ++camera_index;
        }

        // Display camera target texture draw and UI
        {
            auto swapchain_rhi_texture = swapchain->current_texture();
            Texture swapchain_texture{swapchain_rhi_texture};
            auto cmd_encoder = fd.graphics_cmd_pool->get_command_encoder();

            cmd_encoder->resource_barriers({}, {
                rhi::TextureBarrier{
                    .texture = swapchain_rhi_texture,
                    .src_access_type = rhi::ResourceAccessType::present,
                    .dst_access_type = rhi::ResourceAccessType::color_attachment_write,
                },
            });
            displayer->display(cmd_encoder.ref(), swapchain_texture);
            cmd_encoder->resource_barriers({}, {
                rhi::TextureBarrier{
                    .texture = swapchain_rhi_texture,
                    .src_access_type = rhi::ResourceAccessType::color_attachment_write,
                    .dst_access_type = rhi::ResourceAccessType::present,
                },
            });

            std::vector<CRef<rhi::Semaphore>> wait_semaphores{};
            wait_semaphores.reserve(cameras.size() + 1);
            for (auto const& semaphore : fd.camera_semaphores) {
                wait_semaphores.push_back(semaphore.ref());
            }
            wait_semaphores.push_back(fd.acquire_semaphore.ref());

            graphics_queue->submit_command_buffer(
                {cmd_encoder->finish()},
                wait_semaphores,
                {fd.signal_semaphore.ref()},
                fd.fence.ref()
            );
        }

        swapchain->present({fd.signal_semaphore.ref()});
    }

    auto execute_in_this_frame(std::function<auto(Ref<rhi::CommandEncoder>) -> void>&& func) -> void {
        auto cmd_encoder = curr_frame_data().graphics_cmd_pool->get_command_encoder();
        func(cmd_encoder.ref());
        graphics_queue->submit_command_buffer({cmd_encoder->finish()});
    }
    auto execute_immediately(std::function<auto(Ref<rhi::CommandEncoder>) -> void>&& func) -> void {
        auto cmd_encoder = curr_frame_data().graphics_cmd_pool->get_command_encoder();
        func(cmd_encoder.ref());
        graphics_queue->submit_command_buffer({cmd_encoder->finish()}, {}, {}, immediate_execution_fence.ref());
        immediate_execution_fence->wait();
    }

    auto blit_texture_2d(
        Ref<rhi::CommandEncoder> cmd_encoder,
        Ref<Texture> src, uint32_t src_mip_level, uint32_t src_array_layer,
        Ref<Texture> dst, uint32_t dst_mip_level, uint32_t dst_array_layer
    ) -> void {
        command_helpers.blit_2d(cmd_encoder, src, src_mip_level, src_array_layer, dst, dst_mip_level, dst_array_layer);
    }

    auto generate_mipmaps_2d(
        Ref<rhi::CommandEncoder> cmd_encoder,
        Ref<Texture> texture,
        BitFlags<rhi::ResourceAccessType>& texture_access,
        MipmapMode mode
    ) -> void {
        command_helpers.generate_mipmaps_2d(cmd_encoder, texture, texture_access, mode);
    }

    auto compile_pipeline_for_drawable(
        GraphicsPassContext const* graphics_context, CRef<Camera> camera, Ref<Drawable> drawable, Ref<FragmentShader> fs
    ) -> Ref<rhi::GraphicsPipeline> {
        ShaderCompilationEnvironment shader_env;
        drawable->mesh->modify_compiler_environment(shader_env);
        drawable->material->modify_compiler_environment(shader_env);
        fs->modify_compiler_environment(shader_env);
        auto shader_env_id = shader_env.get_config_identifier();

        auto mesh_shaders_id = fmt::format("MESH {} {} ", drawable->mesh->mesh_type_name(), shader_env_id);
        auto fs_id = fmt::format(
            "FS '{}' {} {} {}",
            fs->source_path, fs->source_entry,
            drawable->material->get_shader_identifier(), shader_env_id
        );
        std::string format_id = "FORMAT";
        for (auto format : graphics_context->color_targets_format) {
            format_id = fmt::format("{}-{:x}", format_id, static_cast<uint32_t>(format));
        }
        if (auto format = graphics_context->depth_stencil_format; format != rhi::ResourceFormat::undefined) {
            format_id = fmt::format("{}={:x}", format_id, static_cast<uint32_t>(format));
        }
        auto pipeline_id = fmt::format("{} {} {}", mesh_shaders_id, fs_id, format_id);
        auto pipeline_it = graphics_pipelines.find(pipeline_id);
        if (pipeline_it != graphics_pipelines.end()) {
            return pipeline_it->second.ref();
        }

        auto& mesh_shader_params = drawable->mesh->shader_params_metadata();
        auto& mat_shader_params = drawable->material->shader_params_metadata();
        auto& camera_shader_params = camera->shader_params_metadata();
        shader_env.set_replace_arg(
            "GRAPHICS_MESH_SHADER_PARAMS",
            mesh_shader_params.generated_shader_definition(graphics_set_mesh, graphics_set_samplers)
        );
        shader_env.set_replace_arg(
            "GRAPHICS_MATERIAL_SHADER_PARAMS",
            mesh_shader_params.generated_shader_definition(graphics_set_material, graphics_set_samplers)
        );
        shader_env.set_replace_arg(
            "GRAPHICS_FRAGMENT_SHADER_PARAMS",
            fs->shader_params_metadata.generated_shader_definition(graphics_set_fragment, graphics_set_samplers)
        );
        shader_env.set_replace_arg(
            "GRAPHICS_CAMERA_SHADER_PARAMS",
            camera_shader_params.generated_shader_definition(graphics_set_camera, graphics_set_samplers)
        );

        auto vs_id = mesh_shaders_id + "vs";
        auto vs_it = cached_shaders.find(vs_id);
        if (vs_it == cached_shaders.end()) {
            auto vs = shader_compiler.compile_shader(
                drawable->mesh->source_path(rhi::ShaderStage::vertex),
                drawable->mesh->source_entry(rhi::ShaderStage::vertex),
                rhi::ShaderStage::vertex,
                shader_env
            );
            BI_ASSERT_MSG(vs.has_value(), vs.error());
            vs_it = cached_shaders.insert({std::move(vs_id), vs.value()}).first;
        }
        rhi::PipelineShader pipeline_vs{vs_it->second, drawable->mesh->source_entry(rhi::ShaderStage::vertex)};

        auto compile_mesh_opt_shader = [this, &mesh_shaders_id, &shader_env, &drawable](
            rhi::ShaderStage stage,
            char const* id_suffix,
            Option<rhi::PipelineShader>& pipeline_shader
        ) {
            if (auto entry = drawable->mesh->source_path(stage); !entry.empty()) {
                auto id = mesh_shaders_id + id_suffix;
                auto it = cached_shaders.find(id);
                if (it == cached_shaders.end()) {
                    auto shader = shader_compiler.compile_shader(
                        drawable->mesh->source_path(stage),
                        entry,
                        stage,
                        shader_env
                    );
                    BI_ASSERT_MSG(shader.has_value(), shader.error());
                    it = cached_shaders.insert({std::move(id), shader.value()}).first;
                }
                pipeline_shader = rhi::PipelineShader{it->second, entry};
            }
        };
        Option<rhi::PipelineShader> pipeline_tcs;
        compile_mesh_opt_shader(rhi::ShaderStage::tessellation_control, "tcs", pipeline_tcs);
        Option<rhi::PipelineShader> pipeline_tes;
        compile_mesh_opt_shader(rhi::ShaderStage::tessellation_evaluation, "tes", pipeline_tes);
        Option<rhi::PipelineShader> pipeline_gs;
        compile_mesh_opt_shader(rhi::ShaderStage::geometry, "gs", pipeline_gs);

        auto fs_it = cached_shaders.find(fs_id);
        if (fs_it == cached_shaders.end()) {
            auto shader = shader_compiler.compile_shader(
                fs->source_path,
                fs->source_entry,
                rhi::ShaderStage::fragment,
                shader_env
            );
            BI_ASSERT_MSG(shader.has_value(), shader.error());
            fs_it = cached_shaders.insert({std::move(fs_id), shader.value()}).first;
        }
        rhi::PipelineShader pipeline_fs{fs_it->second, fs->source_entry};

        rhi::GraphicsPipelineDesc pipeline_desc{
            .vertex_input_buffers = drawable->mesh->vertex_input_desc(fs->needed_vertex_attributes),
            .tessellation_state = drawable->mesh->tessellation_desc(),
            .rasterization_state = {
                .topology = drawable->mesh->primitive_topology(),
                .front_face = fs->front_face,
                .cull_mode = fs->cull_mode,
                .polygon_mode = fs->polygon_mode,
                .conservative = fs->conservative_rasterization,
            },
            .depth_stencil_state = {
                .format = graphics_context->depth_stencil_format,
                .depth_write = fs->depth_write,
                .depth_test = fs->depth_test,
                .stencil_test = fs->stencil_test,
                .depth_compare_op = fs->depth_compare_op,
                .stencil_front_face = fs->stencil_front_face,
                .stencil_back_face = fs->stencil_back_face,
                .stencil_compare_mask = fs->stencil_compare_mask,
                .stencil_write_mask = fs->stencil_write_mask,
                .stencil_reference = fs->stencil_reference,
            },
            .shaders = {
                .vertex = pipeline_vs,
                .tessellation_control = pipeline_tcs,
                .tessellation_evaluation = pipeline_tes,
                .geometry = pipeline_gs,
                .fragment = pipeline_fs,
            },
        };
        if (
            drawable->material->blend_mode == BlendMode::translucent
            || drawable->material->blend_mode == BlendMode::additive
            || drawable->material->blend_mode == BlendMode::modulate
        ) {
            pipeline_desc.depth_stencil_state.depth_write = false;
        }

        pipeline_desc.color_target_state.attachments.resize(graphics_context->color_targets_format.size());
        if (!graphics_context->color_targets_format.empty()) {
            auto& color_attachment = pipeline_desc.color_target_state.attachments[0];
            switch (drawable->material->blend_mode) {
                case BlendMode::opaque:
                    break;
                case BlendMode::alpha_test:
                    break;
                case BlendMode::translucent:
                    color_attachment.blend_enable = true;
                    color_attachment.src_blend_factor = rhi::BlendFactor::src_alpha;
                    color_attachment.dst_blend_factor = rhi::BlendFactor::one_minus_src_alpha;
                    color_attachment.src_alpha_blend_factor = rhi::BlendFactor::src_alpha;
                    color_attachment.dst_alpha_blend_factor = rhi::BlendFactor::one_minus_src_alpha;
                    break;
                case BlendMode::additive:
                    color_attachment.blend_enable = true;
                    color_attachment.src_blend_factor = rhi::BlendFactor::one;
                    color_attachment.dst_blend_factor = rhi::BlendFactor::one;
                    color_attachment.src_alpha_blend_factor = rhi::BlendFactor::zero;
                    color_attachment.dst_alpha_blend_factor = rhi::BlendFactor::one;
                    break;
                case BlendMode::modulate:
                    color_attachment.blend_enable = true;
                    color_attachment.src_blend_factor = rhi::BlendFactor::dst;
                    color_attachment.dst_blend_factor = rhi::BlendFactor::zero;
                    color_attachment.src_alpha_blend_factor = rhi::BlendFactor::zero;
                    color_attachment.dst_alpha_blend_factor = rhi::BlendFactor::one;
                    break;
            }
        }

        pipeline_desc.bind_groups_layout.push_back(
            mesh_shader_params.bind_group_layout(
                graphics_set_mesh, BitFlags{rhi::ShaderStage::all_graphics}.clear(rhi::ShaderStage::fragment)
            )
        );
        pipeline_desc.bind_groups_layout.push_back(
            mat_shader_params.bind_group_layout(graphics_set_material, rhi::ShaderStage::fragment)
        );
        pipeline_desc.bind_groups_layout.push_back(
            fs->shader_params_metadata.bind_group_layout(graphics_set_fragment, rhi::ShaderStage::fragment)
        );
        pipeline_desc.bind_groups_layout.push_back(
            camera_shader_params.bind_group_layout(
                graphics_set_camera, BitFlags{rhi::ShaderStage::all_graphics}.clear(rhi::ShaderStage::fragment)
            )
        );
        if (device->properties().separate_sampler_heap) {
            rhi::BindGroupLayout samplers_layout{};
            for (uint32_t set = 0; auto& layout : pipeline_desc.bind_groups_layout) {
                for (auto& entry : layout) {
                    if (entry.type == rhi::DescriptorType::sampler) {
                        samplers_layout.push_back(entry);
                        samplers_layout.back().binding_or_register += samplers_binding_shift * set;
                        samplers_layout.back().space = graphics_set_samplers;
                    }
                }
                layout.erase(
                    std::remove_if(
                        layout.begin(), layout.end(),
                        [](rhi::BindGroupLayoutEntry const& entry) {
                            return entry.type == rhi::DescriptorType::sampler;
                        }
                    ),
                    layout.end()
                );
                ++set;
            }
            if (!samplers_layout.empty()) {
                pipeline_desc.bind_groups_layout.push_back(std::move(samplers_layout));
            }
        }

        pipeline_it = graphics_pipelines.insert(
            {std::move(pipeline_id), device->create_graphics_pipeline(pipeline_desc)}
        ).first;
        return pipeline_it->second.ref();
    }

    auto get_descriptors_for(
        std::vector<rhi::DescriptorHandle>&& cpu_descriptors,
        std::vector<rhi::DescriptorType> const& desc_types,
        rhi::BindGroupLayout const& layout
    ) -> rhi::DescriptorHandle {
        auto& fd = curr_frame_data();
        if (auto it = fd.cached_descriptors.find(cpu_descriptors); it != fd.cached_descriptors.end()) {
            return it->second;
        }

        auto handle = fd.resource_heap->allocate_descriptor(layout);
        device->copy_descriptors(handle, cpu_descriptors, desc_types);
        fd.cached_descriptors.insert({std::move(cpu_descriptors), handle});
        return handle;
    }

    struct FrameData;
    auto curr_frame_data() -> FrameData& {
        return frame_data[g_engine->window()->frame_count() % frame_data.size()];
    }

    Box<rhi::Device> device;
    Ptr<rhi::Queue> graphics_queue;
    Ptr<rhi::Queue> compute_queue;

    ShaderCompiler shader_compiler;
    CommandHelpers command_helpers;

    Box<rhi::Swapchain> swapchain;
    Window::ResizeCallbackHandle swapchain_resize_callback;

    Dyn<IRenderer>::Box renderer;
    Dyn<IDisplayer>::Ptr displayer = nullptr;

    Box<rhi::DescriptorHeap> cpu_resource_heap;
    Box<rhi::DescriptorHeap> cpu_sampler_heap;
    struct FrameData final {
        Box<rhi::Semaphore> acquire_semaphore;
        Box<rhi::Semaphore> signal_semaphore;
        Box<rhi::Fence> fence;
        std::vector<Box<rhi::Semaphore>> camera_semaphores;

        Box<rhi::CommandPool> graphics_cmd_pool;
        Box<rhi::DescriptorHeap> resource_heap;
        Box<rhi::DescriptorHeap> sampler_heap;
        std::unordered_map<std::vector<rhi::DescriptorHandle>, rhi::DescriptorHandle> cached_descriptors;
    };
    std::vector<FrameData> frame_data;
    Box<rhi::Fence> immediate_execution_fence;

    RenderGraph render_graph;
    SlotMap<Camera, CameraHandle> cameras;
    SlotMap<Drawable, DrawableHandle> drawables;
    std::unordered_map<rhi::SamplerDesc, Sampler> samplers;

    StringHashMap<Ref<rhi::ShaderModule>> cached_shaders;
    StringHashMap<Box<rhi::GraphicsPipeline>> graphics_pipelines;
};

GraphicsManager::GraphicsManager() = default;
GraphicsManager::~GraphicsManager() = default;

auto GraphicsManager::initialize(GraphicsSettings const& settings) -> void {
    impl()->initialize(settings);
}

auto GraphicsManager::set_renderer(Dyn<IRenderer>::Box renderer) -> void {
    impl()->set_renderer(std::move(renderer));
}

auto GraphicsManager::set_displayer(Dyn<IDisplayer>::Ref displayer) -> void {
    impl()->set_displayer(displayer);
}

auto GraphicsManager::add_camera() -> CameraHandle {
    return impl()->add_camera();
}
auto GraphicsManager::remove_camera(CameraHandle handle) -> void {
    impl()->remove_camera(handle);
}
auto GraphicsManager::get_camera(CameraHandle handle) -> Ref<Camera> {
    return impl()->get_camera(handle);
}
auto GraphicsManager::for_each_camera(std::function<auto(Camera&) -> void> func) -> void {
    impl()->for_each_camera(std::move(func));
}
auto GraphicsManager::for_each_camera(std::function<auto(Camera const&) -> void> func) const -> void {
    impl()->for_each_camera(std::move(func));
}

auto GraphicsManager::add_drawable() -> DrawableHandle {
    return impl()->add_drawable();
}
auto GraphicsManager::remove_drawable(DrawableHandle handle) -> void {
    impl()->remove_drawable(handle);
}
auto GraphicsManager::get_drawable(DrawableHandle handle) -> Ref<Drawable> {
    return impl()->get_drawable(handle);
}
auto GraphicsManager::for_each_drawable(std::function<auto(Drawable&) -> void> func) -> void {
    impl()->for_each_drawable(std::move(func));
}
auto GraphicsManager::for_each_drawable(std::function<auto(Drawable const&) -> void> func) const -> void {
    impl()->for_each_drawable(std::move(func));
}

auto GraphicsManager::get_sampler(rhi::SamplerDesc const& desc) -> Ref<Sampler> {
    return impl()->get_sampler(desc);
}

auto GraphicsManager::render_frame() -> void {
    impl()->render_frame();
}

auto GraphicsManager::execute_in_this_frame(std::function<auto(Ref<rhi::CommandEncoder>) -> void> func) -> void {
    impl()->execute_in_this_frame(std::move(func));
}
auto GraphicsManager::execute_immediately(std::function<auto(Ref<rhi::CommandEncoder>) -> void> func) -> void {
    impl()->execute_immediately(std::move(func));
}

auto GraphicsManager::blit_texture_2d(
    Ref<rhi::CommandEncoder> cmd_encoder,
    Ref<Texture> src, uint32_t src_mip_level, uint32_t src_array_layer,
    Ref<Texture> dst, uint32_t dst_mip_level, uint32_t dst_array_layer
) -> void {
    impl()->blit_texture_2d(cmd_encoder, src, src_mip_level, src_array_layer, dst, dst_mip_level, dst_array_layer);
}

auto GraphicsManager::generate_mipmaps_2d(
    Ref<rhi::CommandEncoder> cmd_encoder,
    Ref<Texture> texture,
    BitFlags<rhi::ResourceAccessType>& texture_access,
    MipmapMode mode
) -> void {
    impl()->generate_mipmaps_2d(cmd_encoder, texture, texture_access, mode);
}

auto GraphicsManager::device() -> Ref<rhi::Device> {
    return impl()->device.ref();
}

auto GraphicsManager::render_graph() -> RenderGraph& {
    return impl()->render_graph;
}

auto GraphicsManager::num_frames_in_flight() const -> uint32_t {
    return impl()->frame_data.size();
}
auto GraphicsManager::curr_frame_index() const -> uint32_t {
    return g_engine->window()->frame_count() % impl()->frame_data.size();
}

auto GraphicsManager::cpu_resource_descriptor_heap() -> Ref<rhi::DescriptorHeap> {
    return impl()->cpu_resource_heap.ref();
}
auto GraphicsManager::cpu_sampler_descriptor_heap() -> Ref<rhi::DescriptorHeap> {
    return impl()->cpu_sampler_heap.ref();
}

auto GraphicsManager::compile_pipeline_for_drawable(
    GraphicsPassContext const* graphics_context, CRef<Camera> camera, Ref<Drawable> drawable, Ref<FragmentShader> fs
) -> Ref<rhi::GraphicsPipeline> {
    return impl()->compile_pipeline_for_drawable(graphics_context, camera, drawable, fs);
}

auto GraphicsManager::get_descriptors_for(
    std::vector<rhi::DescriptorHandle> cpu_descriptors,
    std::vector<rhi::DescriptorType> const& desc_types,
    rhi::BindGroupLayout const& layout
) -> rhi::DescriptorHandle {
    return impl()->get_descriptors_for(std::move(cpu_descriptors), desc_types, layout);
}

}
