#include "displayer.hpp"

#include <functional>

#include <imgui.h>
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/ecs.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/graphics/camera.hpp>
#include <bisemutum/window/imgui_renderer.hpp>
#include <bisemutum/scene_basic/camera_system.hpp>

namespace bi {

namespace {

auto imgui_window(
    char const* name, std::function<auto(ImVec2, ImVec2) -> void> func,
    ImGuiWindowFlags flags = 0, bool* p_open = nullptr
) -> void {
    auto enabled = ImGui::Begin(name, p_open, flags);
    auto window_pos = ImGui::GetWindowPos();
    auto window_size = ImGui::GetWindowSize();
    if (enabled) {
        func(window_pos, window_size);
    }
    ImGui::End();
}

} // namespace

EditorDisplayer::EditorDisplayer() {
    editor_camera_ = g_engine->graphics_manager()->add_camera();
    auto editor_camera = g_engine->graphics_manager()->get_camera(editor_camera_);
    editor_camera->enabled = false;
}

auto EditorDisplayer::display(Ref<rhi::CommandEncoder> cmd_encoder, Ref<gfx::Texture> target_texture) -> void {
    g_engine->imgui_renderer()->new_frame();

    ImGui::SetNextWindowSize({
        static_cast<float>(target_texture->desc().extent.width),
        static_cast<float>(target_texture->desc().extent.height),
    });
    ImGui::SetNextWindowPos({0.0f, 0.0f});
    ImGui::Begin(
        "##Global", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDocking
    );
    ImGui::DockSpace(ImGui::GetID("Dockspace"));

    auto camera_system = g_engine->ecs_manager()->get_system<CameraSystem>();
    auto scene_camera_handle = camera_system->main_camera_handle();
    if (scene_camera_ != scene_camera_handle) {
        scene_camera_ = scene_camera_handle;
        init_editor_camera();
    }

    if (!is_valid()) { return; }

    imgui_window("Viewport Top Bar", [this](ImVec2 window_pos, ImVec2 window_size) {
        int use_scene_camera = display_scene_camera_;
        ImGui::RadioButton("Editor", &use_scene_camera, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Scene", &use_scene_camera, 1);
        display_scene_camera_ = use_scene_camera;
    });

    auto scene_camera = g_engine->graphics_manager()->get_camera(scene_camera_);
    auto editor_camera = g_engine->graphics_manager()->get_camera(editor_camera_);
    scene_camera->enabled = display_scene_camera_;
    editor_camera->enabled = !display_scene_camera_;
    auto displayed_camera = display_scene_camera_ ? scene_camera : editor_camera;

    imgui_window("Viewport", [displayed_camera](ImVec2 window_pos, ImVec2 window_size) {
        ImGui::Image(&displayed_camera->target_texture(), window_size);
    });

    imgui_window("Hierarchy", [](ImVec2 window_pos, ImVec2 window_size) {
        // TODO
    });

    imgui_window("Properties", [](ImVec2 window_pos, ImVec2 window_size) {
        // TODO
    });

    imgui_window("Contents", [](ImVec2 window_pos, ImVec2 window_size) {
        // TODO
    });

    ImGui::End();
    g_engine->imgui_renderer()->render_draw_data(cmd_encoder, target_texture);
}

auto EditorDisplayer::is_valid() const -> bool {
    return scene_camera_ != gfx::CameraHandle::invalid;
}

auto EditorDisplayer::init_editor_camera() -> void {
    auto scene_camera = g_engine->graphics_manager()->get_camera(scene_camera_);
    auto editor_camera = g_engine->graphics_manager()->get_camera(editor_camera_);

    editor_camera->position = scene_camera->position;
    editor_camera->front_dir = scene_camera->front_dir;
    editor_camera->up_dir = scene_camera->up_dir;
    editor_camera->yfov = scene_camera->yfov;
    editor_camera->near_z = scene_camera->near_z;
    editor_camera->far_z = scene_camera->far_z;
    editor_camera->projection_type = scene_camera->projection_type;
    editor_camera->recreate_target_texture(
        scene_camera->target_texture().desc().extent.width,
        scene_camera->target_texture().desc().extent.height,
        rhi::ResourceFormat::rgba8_unorm, false
    );
}

}
