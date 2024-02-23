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

        // It is needed to manually reset renderer since it may add delayed destroys.
        renderer.reset();
    }

    auto initialize(GraphicsSettings const& settings, std::string_view pipeline_cache_file) -> void {
        device = rhi::Device::create(rhi::DeviceDesc{
            .backend = settings.backend,
            .enable_validation = settings.enable_validation,
        });
        device->initialize_pipeline_cache_from(pipeline_cache_file);

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
        delayed_destroys.resize(settings.num_swapchain_textures);
        frame_data.resize(settings.num_swapchain_textures);
        for (auto& fd : frame_data) {
            fd.acquire_semaphore = device->create_semaphore();
            fd.signal_semaphore = device->create_semaphore();
            fd.fence = device->create_fence();
            fd.graphics_cmd_pool = device->create_command_pool(cmd_pool_desc);
        }
        immediate_execution_fence = device->create_fence();

        render_graph.set_graphics_device(device.ref(), frame_data.size());

        initialize_default_resources();
    }

    auto initialize_default_resources() -> void {
        default_buffer = gfx::Buffer(
            gfx::BufferBuilder{}
                .size(256u)
                .usage({rhi::BufferUsage::uniform, rhi::BufferUsage::storage_read_write})
        );

        auto tex_usages = BitFlags<rhi::TextureUsage>{
            rhi::TextureUsage::sampled, rhi::TextureUsage::storage_read_write,
        };
        default_textures[static_cast<size_t>(DefaultTexture::black_1x1)] = gfx::Texture(
            gfx::TextureBuilder{}
                .dim_2d(rhi::ResourceFormat::rgba8_unorm, 1, 1)
                .usage(tex_usages)
        );
        default_textures[static_cast<size_t>(DefaultTexture::white_1x1)] = gfx::Texture(
            gfx::TextureBuilder{}
                .dim_2d(rhi::ResourceFormat::rgba8_unorm, 1, 1)
                .usage(tex_usages)
        );
        default_textures[static_cast<size_t>(DefaultTexture::normal_1x1)] = gfx::Texture(
            gfx::TextureBuilder{}
                .dim_2d(rhi::ResourceFormat::rgba16_sfloat, 1, 1)
                .usage(tex_usages)
        );
        default_textures[static_cast<size_t>(DefaultTexture::black_1x1_cube)] = gfx::Texture(
            gfx::TextureBuilder{}
                .dim_cube(rhi::ResourceFormat::rgba8_unorm, 1)
                .usage(tex_usages)
        );

        constexpr size_t temp_buffer_size = 4 * 6;
        gfx::Buffer temp_buffer{gfx::BufferBuilder().size(temp_buffer_size).mem_upload()};
        std::array<uint8_t, temp_buffer_size> black{};
        std::fill(black.begin(), black.end(), 0);
        std::array<uint8_t, temp_buffer_size> white{};
        std::fill(white.begin(), white.end(), 255);
        std::array<uint16_t, 4> normal{0x3800, 0x3800, 0x3c00, 0x3c00};

        auto update_texture = [this, &temp_buffer](Texture& texture) {
            execute_immediately([&texture, &temp_buffer](Ref<rhi::CommandEncoder> cmd) {
                cmd->resource_barriers({}, {
                    rhi::TextureBarrier{
                        .texture = texture.rhi_texture(),
                        .dst_access_type = rhi::ResourceAccessType::transfer_write,
                    },
                });
                cmd->copy_buffer_to_texture(
                    temp_buffer.rhi_buffer(),
                    texture.rhi_texture(),
                    rhi::BufferTextureCopyDesc{
                        .buffer_pixels_per_row = 1,
                        .buffer_rows_per_texture = 1,
                    }
                );
                cmd->resource_barriers({}, {
                    rhi::TextureBarrier{
                        .texture = texture.rhi_texture(),
                        .src_access_type = rhi::ResourceAccessType::transfer_write,
                        .dst_access_type = rhi::ResourceAccessType::sampled_texture_read,
                    },
                });
            });
        };
        temp_buffer.set_data_raw(black.data(), temp_buffer_size);
        update_texture(default_textures[static_cast<size_t>(DefaultTexture::black_1x1)]);
        update_texture(default_textures[static_cast<size_t>(DefaultTexture::black_1x1_cube)]);
        temp_buffer.set_data_raw(white.data(), temp_buffer_size);
        update_texture(default_textures[static_cast<size_t>(DefaultTexture::white_1x1)]);
        temp_buffer.set_data_raw(normal.data(), normal.size() * sizeof(uint16_t));
        update_texture(default_textures[static_cast<size_t>(DefaultTexture::normal_1x1)]);
    }

    auto wait_idle() -> void {
        graphics_queue->wait_idle();
        compute_queue->wait_idle();

        for (auto& destroys : delayed_destroys) {
            for (auto& destroy : destroys) {
                destroy();
            }
            destroys.clear();
        }
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

        for (auto& destroy : delayed_destroys[curr_frame_index()]) {
            destroy();
        }
        delayed_destroys[curr_frame_index()].clear();
    }

    auto render_frame() -> void {
        if (!renderer.has_value() || !displayer.has_value()) { return; }

        auto gpu_scene = g_engine->system_manager()->get_system_for_current_scene<GpuSceneSystem>();

        auto& fd = curr_frame_data();
        auto cmd_encoder = fd.graphics_cmd_pool->get_command_encoder();
        set_descriptor_heaps(cmd_encoder);
        curr_cmd_encoder = cmd_encoder.ref();

        renderer.prepare_renderer_per_frame_data();

        gpu_scene->for_each_drawable_with_shader_data(
            [](Drawable& drawable, DrawableShaderData const& drawable_data) {
                drawable.mesh->fill_shader_params(drawable, drawable_data);
                drawable.shader_params.update_uniform_buffer();
                drawable.material->shader_parameters.update_uniform_buffer();
            }
        );
        size_t num_enabled_camera = 0;
        gpu_scene->for_each_camera([&num_enabled_camera](Camera& camera) {
            if (camera.enabled) {
                camera.update_shader_params();
                ++num_enabled_camera;
            }
        });

        // Render each camera
        size_t camera_index = 0;
        gpu_scene->for_each_camera([&cmd_encoder, &camera_index, &fd, this](Camera& camera) {
            if (!camera.enabled) { return; }

            auto label_str = fmt::format("Render Camera {}", camera_index);
            cmd_encoder->push_label(rhi::CommandLabel{
                .label = label_str,
                .color = {1.0f, 1.0f, 0.0f},
            });

            render_graph.set_command_encoder(cmd_encoder.ref());
            render_graph.set_back_buffer(camera.target_texture(), rhi::ResourceAccessType::none);
            renderer.prepare_renderer_per_camera_data(camera);
            renderer.render_camera(camera, render_graph);
            render_graph.execute();

            cmd_encoder->pop_label();

            camera.clear_history_resources();

            ++camera_index;
        });

        // Display camera target texture draw and UI
        {
            auto swapchain_rhi_texture = swapchain->current_texture();
            Texture swapchain_texture{swapchain_rhi_texture};

            cmd_encoder->push_label(rhi::CommandLabel{
                .label = "Display",
                .color = {1.0f, 0.0f, 1.0f},
            });

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
            wait_semaphores.push_back(fd.acquire_semaphore.ref());

            cmd_encoder->pop_label();

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

    auto equitangular_to_cubemap(
        Ref<rhi::CommandEncoder> cmd_encoder,
        Ref<Texture> src, uint32_t src_mip_level, uint32_t src_array_layer,
        Ref<Texture> dst, uint32_t dst_mip_level, uint32_t dst_array_layer
    ) -> void {
        command_helpers.equitangular_to_cubemap(
            cmd_encoder, src, src_mip_level, src_array_layer, dst, dst_mip_level, dst_array_layer
        );
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

    auto update_mesh_buffers(CRef<MeshData> mesh) -> void {
        auto& mesh_buffers = meshes_buffers.try_emplace(mesh->id_).first->second;
        if (mesh_buffers.version < mesh->buffer_version_) {
            mesh_buffers.positions_buffer.reset();

            Buffer::update_with_container(mesh_buffers.positions_buffer, mesh->positions_, rhi::BufferUsage::vertex);
            Buffer::update_with_container(mesh_buffers.normals_buffer, mesh->normals_, rhi::BufferUsage::vertex);
            Buffer::update_with_container(mesh_buffers.tangents_buffer, mesh->tangents_, rhi::BufferUsage::vertex);
            Buffer::update_with_container(mesh_buffers.colors_buffer, mesh->colors_, rhi::BufferUsage::vertex);
            Buffer::update_with_container(mesh_buffers.texcoords_buffer, mesh->texcoords_, rhi::BufferUsage::vertex);
            Buffer::update_with_container(mesh_buffers.texcoords2_buffer, mesh->texcoords2_, rhi::BufferUsage::vertex);
            Buffer::update_with_container(mesh_buffers.indices_buffer, mesh->indices_, rhi::BufferUsage::index);

            mesh_buffers.version = mesh->buffer_version_;
        }
    }

    auto bind_mesh_buffers(
        Ref<rhi::GraphicsCommandEncoder> cmd_encoder, CRef<MeshData> mesh
    ) -> void {
        auto& mesh_buffers = meshes_buffers.try_emplace(mesh->id_).first->second;

        std::vector<Ref<rhi::Buffer>> vertex_buffers;
        if (!mesh->positions_.empty()) {
            vertex_buffers.push_back(mesh_buffers.positions_buffer.rhi_buffer());
        }
        if (!mesh->normals_.empty()) {
            vertex_buffers.push_back(mesh_buffers.normals_buffer.rhi_buffer());
        }
        if (!mesh->tangents_.empty()) {
            vertex_buffers.push_back(mesh_buffers.tangents_buffer.rhi_buffer());
        }
        if (!mesh->colors_.empty()) {
            vertex_buffers.push_back(mesh_buffers.colors_buffer.rhi_buffer());
        }
        if (!mesh->texcoords_.empty()) {
            vertex_buffers.push_back(mesh_buffers.texcoords_buffer.rhi_buffer());
        }
        if (!mesh->texcoords2_.empty()) {
            vertex_buffers.push_back(mesh_buffers.texcoords2_buffer.rhi_buffer());
        }
        if (!vertex_buffers.empty()) {
            cmd_encoder->set_vertex_buffer(vertex_buffers);
        }

        if (!mesh->indices_.empty()) {
            cmd_encoder->set_index_buffer(mesh_buffers.indices_buffer.rhi_buffer(), 0, rhi::IndexType::uint32);
        }
    }
    auto draw_drawable(
        Ref<rhi::GraphicsCommandEncoder> cmd_encoder, Ref<Drawable> drawable
    ) -> void {
        auto& mesh_data = drawable->mesh->get_mesh_data();
        auto& submesh = drawable->submesh_desc();

        auto num_indices = submesh.num_indices;
        if (submesh.num_indices == ~0u) {
            num_indices = mesh_data.indices_.size() - submesh.index_offset;
        }

        if (mesh_data.indices_.empty()) {
            cmd_encoder->draw(num_indices, 1, submesh.base_vertex, 0);
        } else {
            cmd_encoder->draw_indexed(num_indices, 1, submesh.index_offset, submesh.base_vertex, 0);
        }
    }

    auto compile_pipeline_for_drawable(
        GraphicsPassContext const* graphics_context, CRef<Camera> camera, Ref<Drawable> drawable, CRef<FragmentShader> fs
    ) -> Ref<rhi::GraphicsPipeline> {
        ShaderCompilationEnvironment shader_env;
        drawable->mesh->modify_compiler_environment(shader_env);
        if (drawable->material) {
            drawable->material->modify_compiler_environment(shader_env);
        } else {
            shader_env.set_replace_arg("MATERIAL_FUNCTION", "");
        }
        fs->modify_compiler_environment(shader_env);
        auto shader_env_id = shader_env.get_config_identifier();

        std::vector<rhi::VertexInputBufferDesc> vertex_input_desc{};
        std::string vertex_attributes_id{};
        {
            BitFlags<VertexAttributesType> input_vertex_attributes{};
            vertex_attributes_id += "VA-";
            auto& mesh_data = drawable->mesh->get_mesh_data();
            auto add_vertex_attribute = [&vertex_input_desc, &vertex_attributes_id, &input_vertex_attributes](
                auto const& attribute_data,
                rhi::VertexSemantics semantics,
                VertexAttributesType attrib_type,
                std::string_view attribute_id
            ) {
                if (!attribute_data.empty()) {
                    using attribute_type = typename std::remove_reference_t<decltype(attribute_data)>::value_type;
                    rhi::ResourceFormat format;
                    if constexpr (std::is_same_v<attribute_type, float2>) {
                        format = rhi::ResourceFormat::rg32_sfloat;
                    } else if constexpr (std::is_same_v<attribute_type, float3>) {
                        format = rhi::ResourceFormat::rgb32_sfloat;
                    } else if constexpr (std::is_same_v<attribute_type, float4>) {
                        format = rhi::ResourceFormat::rgba32_sfloat;
                    } else {
                        static_assert(traits::AlwaysFalse<attribute_type>, "Invalid vertex attribute type");
                    }
                    vertex_input_desc.push_back(rhi::VertexInputBufferDesc{
                        .stride = sizeof(attribute_type),
                        .attributes = {
                            rhi::VertexInputAttribute{
                                .semantics = semantics,
                                .format = format,
                            }
                        },
                    });
                    vertex_attributes_id += attribute_id;
                    input_vertex_attributes.set(attrib_type);
                }
            };
            add_vertex_attribute(mesh_data.positions_, rhi::VertexSemantics::position, VertexAttributesType::position, "P");
            add_vertex_attribute(mesh_data.normals_, rhi::VertexSemantics::normal, VertexAttributesType::normal, "N");
            add_vertex_attribute(mesh_data.tangents_, rhi::VertexSemantics::tangent, VertexAttributesType::tangent, "T");
            add_vertex_attribute(mesh_data.colors_, rhi::VertexSemantics::color, VertexAttributesType::color, "C");
            add_vertex_attribute(mesh_data.texcoords_, rhi::VertexSemantics::texcoord0, VertexAttributesType::texcoord, "U1");
            add_vertex_attribute(mesh_data.texcoords2_, rhi::VertexSemantics::texcoord0, VertexAttributesType::texcoord2, "U2");

            shader_env.set_define(
                "VERTEX_ATTRIBUTES_IN",
                std::to_string(input_vertex_attributes.raw_value())
            );
        }

        auto mesh_shaders_id = fmt::format(
            "MESH {} {} {} ",
            drawable->mesh->mesh_type_name(),
            vertex_attributes_id,
            shader_env_id
        );
        auto fs_id = fmt::format(
            "FS '{}' {} {} {}",
            fs->source_path,
            fs->source_entry,
            drawable->material ? drawable->material->get_shader_identifier() : "X",
            shader_env_id
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

        auto& mesh_shader_params = drawable->mesh->shader_params_metadata(drawable->submesh_index);
        auto& camera_shader_params = camera->shader_params_metadata();
        shader_env.set_replace_arg(
            "GRAPHICS_MESH_SHADER_PARAMS",
            mesh_shader_params.generated_shader_definition(graphics_set_mesh, graphics_set_samplers)
        );
        shader_env.set_replace_arg(
            "GRAPHICS_FRAGMENT_SHADER_PARAMS",
            fs->shader_params_metadata.generated_shader_definition(graphics_set_fragment, graphics_set_samplers)
        );
        shader_env.set_replace_arg(
            "GRAPHICS_CAMERA_SHADER_PARAMS",
            camera_shader_params.generated_shader_definition(graphics_set_camera, graphics_set_samplers)
        );
        shader_env.set_replace_arg(
            "GRAPHICS_MATERIAL_SHADER_PARAMS",
            drawable->material
                ? drawable->material->shader_params_metadata().generated_shader_definition(
                    graphics_set_material, graphics_set_samplers
                )
                : ""
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
            .vertex_input_buffers = std::move(vertex_input_desc),
            .tessellation_state = drawable->mesh->tessellation_desc(),
            .rasterization_state = {
                .topology = drawable->submesh_desc().topology,
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
            drawable->material
            && (
                drawable->material->blend_mode == BlendMode::translucent
                || drawable->material->blend_mode == BlendMode::additive
                || drawable->material->blend_mode == BlendMode::modulate
            )
        ) {
            pipeline_desc.depth_stencil_state.depth_write = false;
        }

        pipeline_desc.color_target_state.attachments.resize(graphics_context->color_targets_format.size());
        for (size_t index = 0; auto& color_format : graphics_context->color_targets_format) {
            auto& color_attachment = pipeline_desc.color_target_state.attachments[index];
            color_attachment.format = color_format;
            if (drawable->material) {
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
            ++index;
        }

        // This must be in order of set
        pipeline_desc.bind_groups_layout.push_back(
            mesh_shader_params.bind_group_layout(graphics_set_mesh, graphics_set_visibility_mesh)
        );
        if (drawable->material) {
            pipeline_desc.bind_groups_layout.push_back(
                drawable->material->shader_params_metadata().bind_group_layout(
                    graphics_set_material, graphics_set_visibility_material
                )
            );
        } else {
            pipeline_desc.bind_groups_layout.emplace_back();
        }
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

    auto compile_pipeline_compute(CRef<ComputeShader> cs) -> Ref<rhi::ComputePipeline> {
        ShaderCompilationEnvironment shader_env;
        cs->modify_compiler_environment(shader_env);
        auto shader_env_id = shader_env.get_config_identifier();

        auto pipeline_id = fmt::format(
            "CS '{}' {} {}",
            cs->source_path,
            cs->source_entry,
            shader_env_id
        );
        auto pipeline_it = compute_pipelines.find(pipeline_id);
        if (pipeline_it != compute_pipelines.end()) {
            return pipeline_it->second.ref();
        }

        shader_env.set_replace_arg(
            "COMPUTE_SHADER_PARAMS",
            cs->shader_params_metadata.generated_shader_definition(compute_set_normal, compute_set_samplers)
        );

        auto cs_id = pipeline_id + "cs";
        auto cs_it = cached_shaders.find(cs_id);
        if (cs_it == cached_shaders.end()) {
            auto shader = shader_compiler.compile_shader(
                cs->source_path,
                cs->source_entry,
                rhi::ShaderStage::compute,
                shader_env
            );
            BI_ASSERT_MSG(shader.has_value(), shader.error());
            cs_it = cached_shaders.insert({std::move(cs_id), shader.value()}).first;
        }

        rhi::ComputePipelineDesc pipeline_desc{
            .compute = {cs_it->second, cs->source_entry},
        };
        pipeline_desc.bind_groups_layout.push_back(
            cs->shader_params_metadata.bind_group_layout(compute_set_normal, rhi::ShaderStage::compute)
        );
        if (device->properties().separate_sampler_heap) {
            rhi::BindGroupLayout samplers_layout{};
            for (uint32_t set = 0; auto& layout : pipeline_desc.bind_groups_layout) {
                for (auto& entry : layout) {
                    if (entry.type == rhi::DescriptorType::sampler) {
                        samplers_layout.push_back(entry);
                        samplers_layout.back().binding_or_register += samplers_binding_shift * set;
                        samplers_layout.back().space = compute_set_samplers;
                        samplers_layout.back().visibility = rhi::ShaderStage::compute;
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

        pipeline_it = compute_pipelines.insert(
            {std::move(pipeline_id), device->create_compute_pipeline(pipeline_desc)}
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

    auto add_delayed_destroy(MoveOnlyFunction<auto() -> void>&& destroy) -> void {
        delayed_destroys[curr_frame_index()].push_back(std::move(destroy));
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

        Box<rhi::CommandPool> graphics_cmd_pool;
        std::unordered_map<std::vector<rhi::DescriptorHandle>, rhi::DescriptorHandle> cached_descriptors;
    };
    uint32_t frame_index = 0;
    std::vector<FrameData> frame_data;
    Box<rhi::Fence> immediate_execution_fence;
    Ptr<rhi::CommandEncoder> curr_cmd_encoder;

    std::vector<std::vector<MoveOnlyFunction<auto() -> void>>> delayed_destroys;

    RenderGraph render_graph;
    std::unordered_map<rhi::SamplerDesc, Sampler> samplers;

    Buffer default_buffer;
    std::array<Texture, num_default_textures> default_textures;

    StringHashMap<Ref<rhi::ShaderModule>> cached_shaders;
    StringHashMap<Box<rhi::GraphicsPipeline>> graphics_pipelines;
    StringHashMap<Box<rhi::ComputePipeline>> compute_pipelines;

    struct MeshBuffers final {
        uint64_t version = 0;
        Buffer positions_buffer;
        Buffer normals_buffer;
        Buffer tangents_buffer;
        Buffer colors_buffer;
        Buffer texcoords_buffer;
        Buffer texcoords2_buffer;
        Buffer indices_buffer;
    };
    std::unordered_map<uint64_t, MeshBuffers> meshes_buffers;

    struct MeshBlas final {
        uint64_t version = 0;
        Box<rhi::AccelerationStructure> blas;
    };
    std::unordered_map<uint64_t, MeshBlas> meshes_blas;
};

GraphicsManager::GraphicsManager() = default;
GraphicsManager::~GraphicsManager() = default;

auto GraphicsManager::initialize(GraphicsSettings const& settings, std::string_view pipeline_cache_file) -> void {
    impl()->initialize(settings, pipeline_cache_file);
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
auto GraphicsManager::get_renderer() const -> Dyn<IRenderer>::CRef {
    return *&impl()->renderer;
}

auto GraphicsManager::set_displayer(Dyn<IDisplayer>::Ref displayer) -> void {
    impl()->set_displayer(displayer);
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

auto GraphicsManager::equitangular_to_cubemap(
    Ref<rhi::CommandEncoder> cmd_encoder,
    Ref<Texture> src, uint32_t src_mip_level, uint32_t src_array_layer,
    Ref<Texture> dst, uint32_t dst_mip_level, uint32_t dst_array_layer
) -> void {
    impl()->equitangular_to_cubemap(
        cmd_encoder, src, src_mip_level, src_array_layer, dst, dst_mip_level, dst_array_layer
    );
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

auto GraphicsManager::default_buffer() -> Ref<Buffer> {
    return impl()->default_buffer;
}
auto GraphicsManager::default_texture(DefaultTexture index) -> Ref<Texture> {
    return impl()->default_textures[static_cast<size_t>(index)];
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

auto GraphicsManager::update_mesh_buffers(CRef<MeshData> mesh) -> void {
    impl()->update_mesh_buffers(mesh);
}

auto GraphicsManager::bind_mesh_buffers(
    Ref<rhi::GraphicsCommandEncoder> cmd_encoder, CRef<MeshData> mesh
) -> void {
    impl()->bind_mesh_buffers(cmd_encoder, mesh);
}
auto GraphicsManager::draw_drawable(
    Ref<rhi::GraphicsCommandEncoder> cmd_encoder, Ref<Drawable> drawable
) -> void {
    impl()->draw_drawable(cmd_encoder, drawable);
}

auto GraphicsManager::compile_pipeline_for_drawable(
    GraphicsPassContext const* graphics_context, CRef<Camera> camera, Ref<Drawable> drawable, CRef<FragmentShader> fs
) -> Ref<rhi::GraphicsPipeline> {
    return impl()->compile_pipeline_for_drawable(graphics_context, camera, drawable, fs);
}

auto GraphicsManager::compile_pipeline_compute(CRef<ComputeShader> cs) -> Ref<rhi::ComputePipeline> {
    return impl()->compile_pipeline_compute(cs);
}

auto GraphicsManager::get_gpu_descriptor_for(
    std::vector<rhi::DescriptorHandle> cpu_descriptors,
    rhi::BindGroupLayout const& layout
) -> rhi::DescriptorHandle {
    return impl()->get_gpu_descriptor_for(std::move(cpu_descriptors), layout);
}

auto GraphicsManager::add_delayed_destroy(MoveOnlyFunction<auto() -> void> destroy) -> void {
    impl()->add_delayed_destroy(std::move(destroy));
}

}
