#include <bisemutum/graphics/graphics_manager.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/logger.hpp>
#include <bisemutum/runtime/system_manager.hpp>
#include <bisemutum/window/window.hpp>
#include <bisemutum/rhi/device.hpp>
#include <bisemutum/graphics/shader_compiler.hpp>
#include <bisemutum/graphics/render_graph.hpp>
#include <bisemutum/graphics/shader.hpp>
#include <bisemutum/graphics/camera.hpp>
#include <bisemutum/graphics/drawable.hpp>
#include <bisemutum/graphics/gpu_scene_system.hpp>
#include <bisemutum/containers/slotmap.hpp>
#include <bisemutum/prelude/math.hpp>
#include <fmt/format.h>

#include "descriptor_allocator.hpp"
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

constexpr uint32_t cpu_resource_desc_heap_size = 65536;
constexpr uint32_t cpu_sampler_desc_heap_size = 16384;

constexpr uint32_t gpu_resource_desc_heap_size = 65536;
constexpr uint32_t gpu_sampler_desc_heap_size = 2048;
constexpr uint32_t gpu_resource_desc_heap_chunk_size = 16384;
constexpr uint32_t gpu_sampler_desc_heap_chunk_size = 512;

}

struct GraphicsManager::Impl final {
    ~Impl() {
        wait_idle();
    }

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
            .width = window->frame_size().width,
            .height = window->frame_size().height,
            .window_handle = window->platform_handle(),
        });
        swapchain_resize_callback = window->register_resize_callback(
            [this](Window const& window, WindowSize frame_size, WindowSize logic_size) {
                immediate_execution_fence->signal_on(graphics_queue.value());
                immediate_execution_fence->wait();
                swapchain->resize(frame_size.width, frame_size.height);
            }
        );

        cpu_resource_descriptor_allocator = Box<CpuDescriptorAllocator>::make(
            device.ref(), rhi::DescriptorHeapType::resource, cpu_resource_desc_heap_size
        );
        cpu_sampler_descriptor_allocator = Box<CpuDescriptorAllocator>::make(
            device.ref(), rhi::DescriptorHeapType::sampler, cpu_sampler_desc_heap_size
        );

        gpu_resource_descriptor_allocator = Box<GpuDescriptorAllocator>::make(
            device.ref(), rhi::DescriptorHeapType::resource,
            gpu_resource_desc_heap_size, gpu_resource_desc_heap_chunk_size,
            settings.num_swapchain_textures, 1
        );
        gpu_sampler_descriptor_allocator = Box<GpuDescriptorAllocator>::make(
            device.ref(), rhi::DescriptorHeapType::sampler,
            gpu_sampler_desc_heap_size, gpu_sampler_desc_heap_chunk_size,
            settings.num_swapchain_textures, 1
        );

        rhi::CommandPoolDesc cmd_pool_desc{
            .queue = graphics_queue.value(),
        };
        frame_data.resize(settings.num_swapchain_textures);
        for (auto& fd : frame_data) {
            fd.acquire_semaphore = device->create_semaphore();
            fd.signal_semaphore = device->create_semaphore();
            fd.fence = device->create_fence();
            fd.graphics_cmd_pool = device->create_command_pool(cmd_pool_desc);
        }
        immediate_execution_fence = device->create_fence();

        render_graph.set_graphics_device(device.ref(), frame_data.size());
    }

    auto wait_idle() -> void {
        graphics_queue->wait_idle();
        compute_queue->wait_idle();
    }

    auto register_renderer(std::string&& name, std::function<auto() -> Dyn<IRenderer>::Box>&& creator) -> void {
        registered_renderers.insert({std::move(name), std::move(creator)});
    }
    auto set_renderer(std::string_view renderer_type_name) -> void {
        if (auto it = registered_renderers.find(renderer_type_name); it != registered_renderers.end()) {
            renderer = it->second();
        }
    }

    auto set_displayer(Dyn<IDisplayer>::Ref displayer) -> void {
        this->displayer = &displayer;
    }

    auto get_sampler(rhi::SamplerDesc const& desc) -> Ref<Sampler> {
        if (auto it = samplers.find(desc); it != samplers.end()) {
            return it->second;
        }
        auto& sampler = samplers.try_emplace(desc).first->second;
        sampler.initialize(desc);
        return sampler;
    }

    auto new_frame() -> void {
        frame_index = frame_index + 1 == frame_data.size() ? 0 : frame_index + 1;
        auto& fd = curr_frame_data();

        swapchain->acquire_next_texture(fd.acquire_semaphore.ref());
        fd.fence->wait();
        fd.graphics_cmd_pool->reset();
        fd.cached_descriptors.clear();
        gpu_resource_descriptor_allocator->reset(curr_frame_index());
        gpu_sampler_descriptor_allocator->reset(curr_frame_index());
    }

    auto render_frame() -> void {
        if (!renderer.has_value() || !displayer.has_value()) { return; }

        auto gpu_scene = g_engine->system_manager()->get_system_for_current_scene<GpuSceneSystem>();

        auto& fd = curr_frame_data();

        renderer.prepare_renderer_per_frame_data();

        gpu_scene->for_each_drawable([](Drawable& drawable) {
            drawable.mesh->fill_shader_params(drawable);
            drawable.shader_params.update_uniform_buffer();
            drawable.material->shader_parameters.update_uniform_buffer();
        });
        size_t num_enabled_camera = 0;
        gpu_scene->for_each_camera([&num_enabled_camera](Camera& camera) {
            if (camera.enabled) {
                camera.update_shader_params();
                ++num_enabled_camera;
            }
        });

        if (fd.camera_semaphores.size() < num_enabled_camera) {
            for (size_t i = fd.camera_semaphores.size(); i < num_enabled_camera; i++) {
                fd.camera_semaphores.push_back(device->create_semaphore());
            }
        }

        // Render each camera
        size_t camera_index = 0;
        gpu_scene->for_each_camera([&camera_index, &fd, this](Camera& camera) {
            if (!camera.enabled) { return; }

            auto cmd_encoder = fd.graphics_cmd_pool->get_command_encoder();
            set_descriptor_heaps(cmd_encoder);
            curr_cmd_encoder = cmd_encoder.ref();
            render_graph.set_command_encoder(cmd_encoder.ref());

            auto camera_access = camera.target_texture_state_preinitialized_
                ? rhi::ResourceAccessType::none
                : rhi::ResourceAccessType::sampled_texture_read;
            camera.target_texture_state_preinitialized_ = false;
            render_graph.set_back_buffer(camera.target_texture(), camera_access);

            renderer.prepare_renderer_per_camera_data(camera);
            renderer.render_camera(camera, render_graph);
            render_graph.execute();

            curr_cmd_encoder = nullptr;
            graphics_queue->submit_command_buffer(
                {cmd_encoder->finish()},
                {},
                {fd.camera_semaphores[camera_index].ref()}
            );

            ++camera_index;
        });

        // Display camera target texture draw and UI
        {
            auto swapchain_rhi_texture = swapchain->current_texture();
            Texture swapchain_texture{swapchain_rhi_texture};
            auto cmd_encoder = fd.graphics_cmd_pool->get_command_encoder();
            set_descriptor_heaps(cmd_encoder);
            curr_cmd_encoder = cmd_encoder.ref();

            cmd_encoder->resource_barriers({}, {
                rhi::TextureBarrier{
                    .texture = swapchain_rhi_texture,
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
            wait_semaphores.reserve(num_enabled_camera + 1);
            for (size_t i = 0; i < num_enabled_camera; i++) {
                wait_semaphores.push_back(fd.camera_semaphores[i].ref());
            }
            wait_semaphores.push_back(fd.acquire_semaphore.ref());

            curr_cmd_encoder = nullptr;
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
        if (curr_cmd_encoder) {
            BI_ASSERT_MSG(curr_cmd_encoder->valid(), "`execute_in_this_frame()` cannot be called in pass");
            func(curr_cmd_encoder.value());
        } else {
            execute_immediately(std::move(func));
        }
    }
    auto execute_immediately(std::function<auto(Ref<rhi::CommandEncoder>) -> void>&& func) -> void {
        auto& fd = curr_frame_data();
        auto cmd_encoder = fd.graphics_cmd_pool->get_command_encoder();
        set_descriptor_heaps(cmd_encoder);
        func(cmd_encoder.ref());
        graphics_queue->submit_command_buffer({cmd_encoder->finish()}, {}, {}, immediate_execution_fence.ref());
        immediate_execution_fence->wait();
    }

    auto set_descriptor_heaps(Box<rhi::CommandEncoder>& cmd_encoder) -> void {
        cmd_encoder->set_descriptor_heaps({
            gpu_resource_descriptor_allocator->heap(),
            gpu_sampler_descriptor_allocator->heap(),
        });
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

    auto allocate_cpu_descriptor(rhi::DescriptorType type) -> rhi::DescriptorHandle {
        if (type != rhi::DescriptorType::sampler) {
            return cpu_resource_descriptor_allocator->allocate(type);
        } else {
            return cpu_sampler_descriptor_allocator->allocate(type);
        }
    }
    auto free_cpu_resource_descriptor(rhi::DescriptorHandle descriptor) -> void {
        cpu_resource_descriptor_allocator->free(descriptor);
    }
    auto free_cpu_sampler_descriptor(rhi::DescriptorHandle descriptor) -> void {
        cpu_sampler_descriptor_allocator->free(descriptor);
    }

    auto compile_pipeline_for_drawable(
        GraphicsPassContext const* graphics_context, CRef<Camera> camera, Ref<Drawable> drawable, CRef<FragmentShader> fs
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
            mat_shader_params.generated_shader_definition(graphics_set_material, graphics_set_samplers)
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
        for (size_t index = 0; auto& color_format : graphics_context->color_targets_format) {
            auto& color_attachment = pipeline_desc.color_target_state.attachments[index];
            color_attachment.format = color_format;
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
            ++index;
        }

        pipeline_desc.bind_groups_layout.push_back(
            mesh_shader_params.bind_group_layout(graphics_set_mesh, graphics_set_visibility_mesh)
        );
        pipeline_desc.bind_groups_layout.push_back(
            mat_shader_params.bind_group_layout(graphics_set_material, graphics_set_visibility_material)
        );
        pipeline_desc.bind_groups_layout.push_back(
            fs->shader_params_metadata.bind_group_layout(graphics_set_fragment, graphics_set_visibility_fragment)
        );
        pipeline_desc.bind_groups_layout.push_back(
            camera_shader_params.bind_group_layout(graphics_set_camera, graphics_set_visibility_camera)
        );
        if (device->properties().separate_sampler_heap) {
            rhi::BindGroupLayout samplers_layout{};
            for (uint32_t set = 0; auto& layout : pipeline_desc.bind_groups_layout) {
                for (auto& entry : layout) {
                    if (entry.type == rhi::DescriptorType::sampler) {
                        samplers_layout.push_back(entry);
                        samplers_layout.back().binding_or_register += samplers_binding_shift * set;
                        samplers_layout.back().space = graphics_set_samplers;
                        samplers_layout.back().visibility = graphics_set_visibility_samplers;
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

    auto get_gpu_descriptor_for(
        std::vector<rhi::DescriptorHandle>&& cpu_descriptors,
        rhi::BindGroupLayout const& layout
    ) -> rhi::DescriptorHandle {
        auto& fd = curr_frame_data();
        if (auto it = fd.cached_descriptors.find(cpu_descriptors); it != fd.cached_descriptors.end()) {
            return it->second;
        }

        rhi::DescriptorHandle handle;
        if (device->properties().separate_sampler_heap && layout[0].type == rhi::DescriptorType::sampler) {
            handle = gpu_sampler_descriptor_allocator->allocate(layout, curr_frame_index());
        } else {
            handle = gpu_resource_descriptor_allocator->allocate(layout, curr_frame_index());
        }
        device->copy_descriptors(handle, cpu_descriptors, layout);
        fd.cached_descriptors.insert({std::move(cpu_descriptors), handle});
        return handle;
    }

    auto curr_frame_index() const -> uint32_t {
        return frame_index;
    }
    struct FrameData;
    auto curr_frame_data() -> FrameData& {
        return frame_data[curr_frame_index()];
    }

    Box<rhi::Device> device;
    Ptr<rhi::Queue> graphics_queue;
    Ptr<rhi::Queue> compute_queue;

    Box<CpuDescriptorAllocator> cpu_resource_descriptor_allocator;
    Box<CpuDescriptorAllocator> cpu_sampler_descriptor_allocator;

    Box<GpuDescriptorAllocator> gpu_resource_descriptor_allocator;
    Box<GpuDescriptorAllocator> gpu_sampler_descriptor_allocator;

    ShaderCompiler shader_compiler;
    CommandHelpers command_helpers;

    Box<rhi::Swapchain> swapchain;
    Window::ResizeCallbackHandle swapchain_resize_callback;

    StringHashMap<std::function<auto() -> Dyn<IRenderer>::Box>> registered_renderers;
    Dyn<IRenderer>::Box renderer;
    Dyn<IDisplayer>::Ptr displayer = nullptr;

    struct FrameData final {
        Box<rhi::Semaphore> acquire_semaphore;
        Box<rhi::Semaphore> signal_semaphore;
        Box<rhi::Fence> fence;
        std::vector<Box<rhi::Semaphore>> camera_semaphores;

        Box<rhi::CommandPool> graphics_cmd_pool;
        std::unordered_map<std::vector<rhi::DescriptorHandle>, rhi::DescriptorHandle> cached_descriptors;
    };
    uint32_t frame_index = 0;
    std::vector<FrameData> frame_data;
    Box<rhi::Fence> immediate_execution_fence;
    Ptr<rhi::CommandEncoder> curr_cmd_encoder;

    RenderGraph render_graph;
    // SlotMap<Camera, CameraHandle> cameras;
    // SlotMap<Drawable, DrawableHandle> drawables;
    std::unordered_map<rhi::SamplerDesc, Sampler> samplers;

    StringHashMap<Ref<rhi::ShaderModule>> cached_shaders;
    StringHashMap<Box<rhi::GraphicsPipeline>> graphics_pipelines;
};

GraphicsManager::GraphicsManager() = default;
GraphicsManager::~GraphicsManager() = default;

auto GraphicsManager::initialize(GraphicsSettings const& settings) -> void {
    impl()->initialize(settings);
}

auto GraphicsManager::wait_idle() -> void {
    impl()->wait_idle();
}

auto GraphicsManager::new_frame() -> void {
    impl()->new_frame();
}

auto GraphicsManager::register_renderer(
    std::string&& name, std::function<auto() -> Dyn<IRenderer>::Box> creator
) -> void {
    impl()->register_renderer(std::move(name), std::move(creator));
}
auto GraphicsManager::set_renderer(std::string_view renderer_type_name) -> void {
    impl()->set_renderer(renderer_type_name);
}

auto GraphicsManager::set_displayer(Dyn<IDisplayer>::Ref displayer) -> void {
    impl()->set_displayer(displayer);
}

// auto GraphicsManager::add_camera() -> CameraHandle {
//     return impl()->add_camera();
// }
// auto GraphicsManager::remove_camera(CameraHandle handle) -> void {
//     impl()->remove_camera(handle);
// }
// auto GraphicsManager::get_camera(CameraHandle handle) -> Ref<Camera> {
//     return impl()->get_camera(handle);
// }
// auto GraphicsManager::for_each_camera(std::function<auto(Camera&) -> void> func) -> void {
//     impl()->for_each_camera(std::move(func));
// }
// auto GraphicsManager::for_each_camera(std::function<auto(Camera const&) -> void> func) const -> void {
//     impl()->for_each_camera(std::move(func));
// }

// auto GraphicsManager::add_drawable() -> DrawableHandle {
//     return impl()->add_drawable();
// }
// auto GraphicsManager::remove_drawable(DrawableHandle handle) -> void {
//     impl()->remove_drawable(handle);
// }
// auto GraphicsManager::get_drawable(DrawableHandle handle) -> Ref<Drawable> {
//     return impl()->get_drawable(handle);
// }
// auto GraphicsManager::for_each_drawable(std::function<auto(Drawable&) -> void> func) -> void {
//     impl()->for_each_drawable(std::move(func));
// }
// auto GraphicsManager::for_each_drawable(std::function<auto(Drawable const&) -> void> func) const -> void {
//     impl()->for_each_drawable(std::move(func));
// }

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

auto GraphicsManager::shader_compiler() -> Ref<ShaderCompiler> {
    return impl()->shader_compiler;
}

auto GraphicsManager::render_graph() -> RenderGraph& {
    return impl()->render_graph;
}

auto GraphicsManager::swapchain_format() const -> rhi::ResourceFormat {
    return impl()->swapchain->format();
}

auto GraphicsManager::num_frames_in_flight() const -> uint32_t {
    return impl()->frame_data.size();
}
auto GraphicsManager::curr_frame_index() const -> uint32_t {
    return impl()->curr_frame_index();
}

auto GraphicsManager::allocate_cpu_descriptor(rhi::DescriptorType type) -> rhi::DescriptorHandle {
    return impl()->allocate_cpu_descriptor(type);
}
auto GraphicsManager::free_cpu_resource_descriptor(rhi::DescriptorHandle descriptor) -> void {
    impl()->free_cpu_resource_descriptor(descriptor);
}
auto GraphicsManager::free_cpu_sampler_descriptor(rhi::DescriptorHandle descriptor) -> void {
    impl()->free_cpu_sampler_descriptor(descriptor);
}

auto GraphicsManager::compile_pipeline_for_drawable(
    GraphicsPassContext const* graphics_context, CRef<Camera> camera, Ref<Drawable> drawable, CRef<FragmentShader> fs
) -> Ref<rhi::GraphicsPipeline> {
    return impl()->compile_pipeline_for_drawable(graphics_context, camera, drawable, fs);
}

auto GraphicsManager::get_gpu_descriptor_for(
    std::vector<rhi::DescriptorHandle> cpu_descriptors,
    rhi::BindGroupLayout const& layout
) -> rhi::DescriptorHandle {
    return impl()->get_gpu_descriptor_for(std::move(cpu_descriptors), layout);
}

}
