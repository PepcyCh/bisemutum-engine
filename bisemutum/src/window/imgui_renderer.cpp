#include <bisemutum/window/imgui_renderer.hpp>

#include <unordered_map>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/window/window.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/graphics/shader_compiler.hpp>
#include <bisemutum/graphics/sampler.hpp>
#include <bisemutum/rhi/device.hpp>
#include <bisemutum/runtime/logger.hpp>

namespace bi {

namespace {

constexpr float imgui_font_size = 14.0f;

}

struct ImGuiRenderer::Impl final {
    auto initialize(Window& window, gfx::GraphicsManager& gfx_mgr) -> void {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        auto dpi_scale = window.dpi_scale();
        ImFontConfig font_cfg{};
        font_cfg.SizePixels = imgui_font_size * dpi_scale;
        imgui_curr_font = io.Fonts->AddFontDefault(&font_cfg);
        imgui_fonts[dpi_scale] = imgui_curr_font;

        ImGui::StyleColorsClassic();
        imgui_last_scale = dpi_scale;
        ImGui::GetStyle().ScaleAllSizes(dpi_scale);

        imgui_font_resize_callback = window.register_resize_callback(
            [this](Window const& window, WindowSize frame_size, WindowSize logic_size) {
                if (g_engine->graphics_manager()->swapchain_format() != swapchain_format) {
                    create_pipeline(*g_engine->graphics_manager());
                }

                auto dpi_scale = window.dpi_scale();
                auto rel_scale = dpi_scale / imgui_last_scale;
                ImGui::GetStyle().ScaleAllSizes(rel_scale);
                imgui_last_scale = dpi_scale;
                if (auto it = imgui_fonts.find(dpi_scale); it != imgui_fonts.end()) {
                    imgui_curr_font = it->second;
                } else {
                    auto& io = ImGui::GetIO();
                    ImFontConfig font_cfg{};
                    font_cfg.SizePixels = imgui_font_size * dpi_scale;
                    auto font = io.Fonts->AddFontDefault(&font_cfg);
                    imgui_fonts[dpi_scale] = font;
                    imgui_curr_font = font;
                    io.Fonts->Build();
                    create_font_texture(*g_engine->graphics_manager());
                }
            }
        );

        switch (gfx_mgr.device()->get_backend()) {
            case rhi::Backend::vulkan:
                ImGui_ImplGlfw_InitForVulkan(window.raw_glfw_window(), true);
                break;
            default:
                ImGui_ImplGlfw_InitForOther(window.raw_glfw_window(), true);
                break;
        }

        create_device_objects(gfx_mgr);
        create_font_texture(gfx_mgr);
    }

    auto finalize() -> void {
        destroy_device_objects();

        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    auto new_frame() -> void {
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::PushFont(imgui_curr_font);
    }

    auto create_device_objects(gfx::GraphicsManager& gfx_mgr) -> void {
        rhi::SamplerDesc sampler_desc{
            .mag_filter = rhi::SamplerFilterMode::linear,
            .min_filter = rhi::SamplerFilterMode::linear,
            .mipmap_mode = rhi::SamplerMipmapMode::linear,
            .address_mode_u = rhi::SamplerAddressMode::repeat,
            .address_mode_v = rhi::SamplerAddressMode::repeat,
            .address_mode_w = rhi::SamplerAddressMode::repeat,
            .anisotropy = 1.0f,
        };
        font_sampler = gfx_mgr.get_sampler(sampler_desc);

        create_pipeline(gfx_mgr);
    }
    auto create_pipeline(gfx::GraphicsManager& gfx_mgr) -> void {
        if (imgui_pipeline) {
            imgui_pipeline.reset();
        }

        auto source_path = "/bisemutum/shaders/imgui/imgui.hlsl";
        gfx::ShaderCompilationEnvironment shader_env;
        auto imgui_vs = gfx_mgr.shader_compiler()->compile_shader(
            source_path, "imgui_vs", rhi::ShaderStage::vertex, shader_env
        );
        BI_ASSERT_MSG(imgui_vs.has_value(), imgui_vs.error());
        auto imgui_fs = gfx_mgr.shader_compiler()->compile_shader(
            source_path, "imgui_fs", rhi::ShaderStage::fragment, shader_env
        );
        BI_ASSERT_MSG(imgui_fs.has_value(), imgui_fs.error());

        rhi::BindGroupLayout layout{
            rhi::BindGroupLayoutEntry{
                .count = 1,
                .type = rhi::DescriptorType::sampled_texture,
                .visibility = rhi::ShaderStage::fragment,
                .binding_or_register = 0,
                .space = 0,
            },
        };
        rhi::StaticSampler static_sampler{
            .sampler = font_sampler->rhi_sampler(),
            .binding_or_register = 0,
            .space = 1,
            .visibility = rhi::ShaderStage::fragment,
        };
        swapchain_format = gfx_mgr.swapchain_format();
        rhi::GraphicsPipelineDesc pipeline_desc{
            .bind_groups_layout = {std::move(layout)},
            .static_samplers = {std::move(static_sampler)},
            .push_constants = rhi::PushConstantsDesc{
                .size = sizeof(float) * 4,
                .visibility = rhi::ShaderStage::vertex,
                .register_ = 1,
                .space = 0,
            },
            .vertex_input_buffers = {
                rhi::VertexInputBufferDesc{
                    .stride = sizeof(ImDrawVert),
                    .attributes = {
                        rhi::VertexInputAttribute{
                            .offset = 0,
                            .semantics = rhi::VertexSemantics::position,
                            .format = rhi::ResourceFormat::rg32_sfloat,
                        },
                        rhi::VertexInputAttribute{
                            .offset = sizeof(float) * 2,
                            .semantics = rhi::VertexSemantics::texcoord0,
                            .format = rhi::ResourceFormat::rg32_sfloat,
                        },
                        rhi::VertexInputAttribute{
                            .offset = sizeof(float) * 4,
                            .semantics = rhi::VertexSemantics::color,
                            .format = rhi::ResourceFormat::rgba8_unorm,
                        },
                    },
                },
            },
            .color_target_state = {
                .attachments = {
                    rhi::ColorTargetAttachmentState{
                        .format = swapchain_format,
                        .blend_enable = true,
                        .blend_op = rhi::BlendOp::add,
                        .src_blend_factor = rhi::BlendFactor::src_alpha,
                        .dst_blend_factor = rhi::BlendFactor::one_minus_src_alpha,
                        .alpha_blend_op = rhi::BlendOp::add,
                        .src_alpha_blend_factor = rhi::BlendFactor::src_alpha,
                        .dst_alpha_blend_factor = rhi::BlendFactor::one_minus_src_alpha,
                    },
                },
            },
            .shaders = {
                .vertex = {imgui_vs.value(), "imgui_vs"},
                .fragment = {imgui_fs.value(), "imgui_fs"},
            },
        };
        imgui_pipeline = gfx_mgr.device()->create_graphics_pipeline(pipeline_desc);
    }
    auto create_font_texture(gfx::GraphicsManager& gfx_mgr) -> void {
        if (font_texture.has_value()) {
            font_texture.reset();
        }

        auto& io = ImGui::GetIO();
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        size_t upload_size = width * height * 4 * sizeof(char);

        rhi::TextureDesc texture_desc{
            .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
            .levels = 1,
            .format = rhi::ResourceFormat::rgba8_unorm,
            .dim = rhi::TextureDimension::d2,
            .usages = {rhi::TextureUsage::sampled},
        };
        font_texture = gfx::Texture{texture_desc};

        rhi::BufferDesc temp_buffer_desc{
            .size = upload_size,
            .memory_property = rhi::BufferMemoryProperty::cpu_to_gpu,
        };
        gfx::Buffer temp_buffer{temp_buffer_desc};
        temp_buffer.set_data_raw(pixels, upload_size);

        gfx_mgr.execute_immediately([this, &temp_buffer, width, height](Ref<rhi::CommandEncoder> cmd) {
            cmd->resource_barriers({}, {
                rhi::TextureBarrier{
                    .texture = font_texture.rhi_texture(),
                    .dst_access_type = rhi::ResourceAccessType::transfer_write,
                },
            });
            cmd->copy_buffer_to_texture(
                temp_buffer.rhi_buffer(),
                font_texture.rhi_texture(),
                rhi::BufferTextureCopyDesc{
                    .buffer_pixels_per_row = static_cast<uint32_t>(width),
                    .buffer_rows_per_texture = static_cast<uint32_t>(height),
                }
            );
            cmd->resource_barriers({}, {
                rhi::TextureBarrier{
                    .texture = font_texture.rhi_texture(),
                    .src_access_type = rhi::ResourceAccessType::transfer_write,
                    .dst_access_type = rhi::ResourceAccessType::sampled_texture_read,
                },
            });
        });

        io.Fonts->SetTexID(&font_texture);
    }

    auto destroy_device_objects() -> void {
        imgui_pipeline.reset();
        font_texture.reset();
        font_sampler->reset();
    }

    auto render_draw_data(Ref<rhi::CommandEncoder> cmd_encoder, Ref<gfx::Texture> target_texture) -> void {
        ImGui::PopFont();
        ImGui::Render();
        auto draw_data = ImGui::GetDrawData();
        auto fb_width = static_cast<int>(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
        auto fb_height = static_cast<int>(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
        if (fb_width <= 0 || fb_height <= 0) { return; }

        if (draw_data->TotalVtxCount > 0) {
            auto vertex_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
            if (!vertex_buffer.has_value()) {
                vertex_buffer = gfx::Buffer{
                    rhi::BufferDesc{
                        .size = vertex_size,
                        .usages = rhi::BufferUsage::vertex,
                        .memory_property = rhi::BufferMemoryProperty::cpu_to_gpu,
                    }
                };
            } else if (vertex_buffer.desc().size < vertex_size) {
                vertex_buffer.resize(vertex_size);
            }
            uint64_t vertex_offset = 0;

            auto index_size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);
            if (!index_buffer.has_value()) {
                index_buffer = gfx::Buffer{
                    rhi::BufferDesc{
                        .size = index_size,
                        .usages = rhi::BufferUsage::index,
                        .memory_property = rhi::BufferMemoryProperty::cpu_to_gpu,
                    }
                };
            } else if (index_buffer.desc().size < index_size) {
                index_buffer.resize(index_size);
            }
            uint64_t index_offset = 0;

            for (int n = 0; n < draw_data->CmdListsCount; n++) {
                auto cmd_list = draw_data->CmdLists[n];
                vertex_buffer.set_data(cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size, vertex_offset);
                index_buffer.set_data(cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size, index_offset);
                vertex_offset += cmd_list->VtxBuffer.Size * sizeof(ImDrawVert);
                index_offset += cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx);
            }
        } else {
            return;
        }

        auto graphics_encoder = cmd_encoder->begin_render_pass(
            rhi::CommandLabel{
                .label = "ImGui",
                .color = {0.0f, 0.0f, 1.0f},
            },
            rhi::RenderTargetDesc{
                .colors = {
                    rhi::ColorAttachmentDesc{
                        .texture = {target_texture->rhi_texture()},
                        .clear_color = {0.0f, 0.0f, 0.0f},
                        .clear = true,
                        .store = true,
                    },
                },
            }
        );

        rhi::Viewport viewport{
            .width = static_cast<float>(fb_width),
            .height = static_cast<float>(fb_height),
        };
        graphics_encoder->set_viewports({viewport});

        graphics_encoder->set_pipeline(imgui_pipeline.ref());
        graphics_encoder->set_vertex_buffer({vertex_buffer.rhi_buffer()});
        graphics_encoder->set_index_buffer(index_buffer.rhi_buffer(), 0, rhi::IndexType::uint16);

        float scale_translate[4];
        scale_translate[0] = 2.0f / draw_data->DisplaySize.x;
        scale_translate[1] = 2.0f / draw_data->DisplaySize.y;
        scale_translate[2] = -1.0f - draw_data->DisplayPos.x * scale_translate[0];
        scale_translate[3] = -1.0f - draw_data->DisplayPos.y * scale_translate[1];
        graphics_encoder->push_constants(scale_translate, sizeof(scale_translate));

        auto clip_off = draw_data->DisplayPos;
        auto clip_scale = draw_data->FramebufferScale;
        int global_vtx_offset = 0;
        int global_idx_offset = 0;
        for (int n = 0; n < draw_data->CmdListsCount; n++) {
            auto cmd_list = draw_data->CmdLists[n];
            for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
                auto pcmd = &cmd_list->CmdBuffer[cmd_i];
                if (pcmd->UserCallback != nullptr) {
                    // User callback not supported currently
                } else {
                    ImVec2 clip_min(
                        (pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y
                    );
                    ImVec2 clip_max(
                        (pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y
                    );

                    if (clip_min.x < 0.0f) { clip_min.x = 0.0f; }
                    if (clip_min.y < 0.0f) { clip_min.y = 0.0f; }
                    if (clip_max.x > fb_width) { clip_max.x = fb_width; }
                    if (clip_max.y > fb_height) { clip_max.y = fb_height; }
                    if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y) { continue; }

                    rhi::Scissor scissor{
                        .x = static_cast<int>(clip_min.x),
                        .y = static_cast<int>(clip_min.y),
                        .width = static_cast<uint32_t>(clip_max.x - clip_min.x),
                        .height = static_cast<uint32_t>(clip_max.y - clip_min.y),
                    };
                    graphics_encoder->set_scissors({scissor});

                    auto texture = static_cast<gfx::Texture*>(pcmd->TextureId);
                    if (texture) {
                        auto descriptor = g_engine->graphics_manager()->get_descriptors_for(
                            {texture->get_srv(0, 1, 0, 1)},
                            imgui_pipeline->desc().bind_groups_layout[0]
                        );
                        graphics_encoder->set_descriptors(0, {descriptor});

                        graphics_encoder->draw_indexed(
                            pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset
                        );
                    }
                }
            }
            global_idx_offset += cmd_list->IdxBuffer.Size;
            global_vtx_offset += cmd_list->VtxBuffer.Size;
        }
    }

    Window::ResizeCallbackHandle imgui_font_resize_callback;
    std::unordered_map<float, ImFont*> imgui_fonts;
    ImFont* imgui_curr_font = nullptr;
    float imgui_last_scale = 1.0f;

    Ptr<gfx::Sampler> font_sampler = nullptr;
    gfx::Texture font_texture;
    Box<rhi::GraphicsPipeline> imgui_pipeline;
    gfx::Buffer vertex_buffer;
    gfx::Buffer index_buffer;
    rhi::ResourceFormat swapchain_format = rhi::ResourceFormat::undefined;
};

ImGuiRenderer::ImGuiRenderer() = default;
ImGuiRenderer::~ImGuiRenderer() = default;

auto ImGuiRenderer::initialize(Window& window, gfx::GraphicsManager& gfx_mgr) -> void {
    impl()->initialize(window, gfx_mgr);
}

auto ImGuiRenderer::finalize() -> void {
    impl()->finalize();
}

auto ImGuiRenderer::new_frame() -> void {
    impl()->new_frame();
}

auto ImGuiRenderer::render_draw_data(Ref<rhi::CommandEncoder> cmd_encoder, Ref<gfx::Texture> target_texture) -> void {
    impl()->render_draw_data(cmd_encoder, target_texture);
}

}
