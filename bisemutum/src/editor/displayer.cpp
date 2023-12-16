#include "displayer.hpp"

#include <functional>

#include <imgui.h>
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/world.hpp>
#include <bisemutum/runtime/scene.hpp>
#include <bisemutum/runtime/component_manager.hpp>
#include <bisemutum/runtime/system_manager.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/graphics/gpu_scene_system.hpp>
#include <bisemutum/graphics/camera.hpp>
#include <bisemutum/window/window.hpp>
#include <bisemutum/window/imgui_renderer.hpp>
#include <bisemutum/editor/menu_manager.hpp>
#include <bisemutum/scene_basic/camera_system.hpp>

namespace bi {

namespace {

constexpr rhi::ResourceFormat editor_camera_format = rhi::ResourceFormat::rgba16_sfloat;

} // namespace

auto EditorDisplayer::init_displayer() -> void {
    auto gpu_scene = g_engine->system_manager()->get_system_for_current_scene<gfx::GpuSceneSystem>();

    // TODO - There should be one editor camera for each scene
    editor_camera_ = gpu_scene->add_camera();
    auto editor_camera = gpu_scene->get_camera(editor_camera_);
    editor_camera->enabled = false;

    editor_camera_resize_ = g_engine->window_manager()->register_resize_callback(
        g_engine->window_manager()->main_viewport_name(),
        [this](WindowManager const& window, WindowSize frame_size, WindowSize logic_size) {
            auto gpu_scene = g_engine->system_manager()->get_system_for_current_scene<gfx::GpuSceneSystem>();
            auto editor_camera = gpu_scene->get_camera(editor_camera_);
            if (editor_camera->enabled) {
                g_engine->graphics_manager()->wait_idle();
                editor_camera->recreate_target_texture(
                    logic_size.width, logic_size.height, editor_camera_format, false
                );
            }
        }
    );
}

auto EditorDisplayer::display(Ref<rhi::CommandEncoder> cmd_encoder, Ref<gfx::Texture> target_texture) -> void {
    if (editor_camera_ == gfx::CameraHandle::invalid) { init_displayer(); }

    g_engine->imgui_renderer()->new_frame();

    auto& io = ImGui::GetIO();

    if (ImGui::BeginMainMenuBar()) {
        editor::MenuActionContext ctx{
            .file_dialog = file_dialog_,
        };
        g_engine->menu_manager()->display(ctx);
        ImGui::EndMainMenuBar();
    }

    ImGui::SetNextWindowSize({
        static_cast<float>(target_texture->desc().extent.width),
        static_cast<float>(target_texture->desc().extent.height),
    });
    ImGui::SetNextWindowPos({0.0f, 20.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    ImGui::Begin(
        "##Global", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDocking
    );
    ImGui::DockSpace(ImGui::GetID("Dockspace"));
    ImGui::PopStyleVar();

    auto camera_system = g_engine->system_manager()->get_system_for_current_scene<CameraSystem>();
    auto scene_camera_handle = camera_system->main_camera_handle();
    if (scene_camera_ != scene_camera_handle) {
        scene_camera_ = scene_camera_handle;
        init_editor_camera();
    }

    if (!is_valid()) {
        ImGui::End();
        g_engine->imgui_renderer()->render_draw_data(cmd_encoder, target_texture);
        return;
    }

    auto wm = g_engine->window_manager();

    auto display_scene_camera_this_frame = display_scene_camera_;
    wm->imgui_window("Viewport Top Bar", [this, &io](ImGuiWindowArgs const& args) {
        int use_scene_camera = display_scene_camera_;
        ImGui::Text("%.3f ms | %.1f FPS", 1000.0f / io.Framerate, io.Framerate);
        ImGui::SameLine();
        ImGui::RadioButton("Editor", &use_scene_camera, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Scene", &use_scene_camera, 1);
        display_scene_camera_ = use_scene_camera;
    });

    auto gpu_scene = g_engine->system_manager()->get_system_for_current_scene<gfx::GpuSceneSystem>();
    auto scene_camera = gpu_scene->get_camera(scene_camera_);
    auto editor_camera = gpu_scene->get_camera(editor_camera_);
    scene_camera->enabled = display_scene_camera_;
    editor_camera->enabled = !display_scene_camera_;
    auto displayed_camera = display_scene_camera_this_frame ? scene_camera : editor_camera;

    if (!display_scene_camera_this_frame) {
        move_editor_camera(editor_camera, io.DeltaTime);
    }

    wm->imgui_window(
        g_engine->window_manager()->main_viewport_name(),
        [displayed_camera](ImGuiWindowArgs const& args) {
            ImGui::Image(
                &displayed_camera->target_texture(),
                {static_cast<float>(args.window_size.x), static_cast<float>(args.window_size.y)}
            );
        },
        ImGuiWindowConfig{
            .padding = 0.0f,
        }
    );

    wm->imgui_window("Hierarchy", [this](ImGuiWindowArgs const& args) {
        auto scene = g_engine->world()->current_scene();
        if (!scene) { return; }
        std::function<auto(Ref<rt::SceneObject>) -> void> object_visitor =
            [this, &object_visitor](Ref<rt::SceneObject> object) {
                auto id = object->get_id();
                auto is_selected = selected_object_ == object;
                auto tree_flags = ImGuiTreeNodeFlags_SpanAvailWidth
                    | (is_selected ? ImGuiTreeNodeFlags_Selected : 0);
                auto tree_open = ImGui::TreeNodeEx(
                    reinterpret_cast<void*>(id), tree_flags, "%s", object->get_name_cstr()
                );
                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                    selected_object_ = object;
                }
                if (tree_open) {
                    object->for_each_children(object_visitor);
                    ImGui::TreePop();
                }
            };
        scene->for_each_root_object(object_visitor);
    });

    wm->imgui_window("Properties", [this](ImGuiWindowArgs const& args) {
        if (!selected_object_) {
            ImGui::Text("No object is selected.");
        } else {
            ImGui::Text("%s", selected_object_->get_name_cstr());
            bool edited = false;
            selected_object_->for_each_component([this, &edited](std::string_view component_type_name, void* value) {
                // if (ImGui::TreeNode(component_type_name.data())) {
                if (ImGui::CollapsingHeader(component_type_name.data())) {
                    auto& editor_func = g_engine->component_manager()->get_editor(component_type_name);
                    edited = editor_func(selected_object_.value(), value);
                    // ImGui::TreePop();
                }
            });
        }
    });

    wm->imgui_window("Contents", [](ImGuiWindowArgs const& args) {
        // TODO - contents viewer
    });

    file_dialog_.update();

    ImGui::End();
    g_engine->imgui_renderer()->render_draw_data(cmd_encoder, target_texture);
}

auto EditorDisplayer::is_valid() const -> bool {
    return scene_camera_ != gfx::CameraHandle::invalid;
}

auto EditorDisplayer::init_editor_camera() -> void {
    auto gpu_scene = g_engine->system_manager()->get_system_for_current_scene<gfx::GpuSceneSystem>();
    auto scene_camera = gpu_scene->get_camera(scene_camera_);
    auto editor_camera = gpu_scene->get_camera(editor_camera_);

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
        editor_camera_format, false
    );
}

auto EditorDisplayer::move_editor_camera(Ref<gfx::Camera> camera, float delta_time) -> void {
    auto ws = g_engine->window();

    float angle_horizontal = 0.0f;
    float angle_vertical = 0.0f;
    static constexpr float turn_speed = 2.0f;

    if (!camera_turning_ && ws->mouse_state(input::Mouse::right) == input::KeyState::press) {
        camera_turning_ = true;
        camera_turning_from_ = ws->cursor_pos();
    }
    if (camera_turning_ && ws->mouse_state(input::Mouse::right) == input::KeyState::press) {
        auto pos = ws->cursor_pos();
        angle_horizontal = turn_speed * (camera_turning_from_.x - pos.x) / ws->frame_size().width / ws->dpi_scale();
        angle_vertical = turn_speed * (camera_turning_from_.y - pos.y) / ws->frame_size().height / ws->dpi_scale();
        camera_turning_from_ = pos;
    }
    if (camera_turning_ && ws->mouse_state(input::Mouse::right) == input::KeyState::release) {
        camera_turning_ = false;
    }

    auto front = camera->front_dir;
    auto up = camera->up_dir;
    if (angle_horizontal != 0.0f) {
        auto mat = math::rotate(float4x4(1.0f), angle_horizontal, float3(0, 1, 0));
        front = mat * float4(front.x, front.y, front.z, 0.0f);
        up = mat * float4(up.x, up.y, up.z, 0.0f);
    }
    if (angle_vertical != 0.0f) {
        auto right = math::cross(front, up);
        auto mat = math::rotate(float4x4(1.0f), angle_vertical, float3(right.x, right.y, right.z));
        front = mat * float4(front.x, front.y, front.z, 0.0f);
        right = math::normalize(math::cross(front, up));
        up = math::cross(right, front);
    }
    auto right = math::cross(front, up);

    float slow_factor = 1.0f;
    if (ws->key_state(input::Keyboard::left_shift) == input::KeyState::press) {
        slow_factor = 0.2f;
    }
    auto pos = camera->position;
    static constexpr float move_speed = 10.0f;
    if (
        ws->key_state(input::Keyboard::w) == input::KeyState::press
        || ws->key_state(input::Keyboard::up) == input::KeyState::press
    ) {
        pos += move_speed * slow_factor * delta_time * front;
    }
    if (
        ws->key_state(input::Keyboard::s) == input::KeyState::press
        || ws->key_state(input::Keyboard::down) == input::KeyState::press
    ) {
        pos -= move_speed * slow_factor * delta_time * front;
    }
    if (
        ws->key_state(input::Keyboard::a) == input::KeyState::press
        || ws->key_state(input::Keyboard::left) == input::KeyState::press
    ) {
        pos -= move_speed * slow_factor * delta_time * right;
    }
    if (
        ws->key_state(input::Keyboard::d) == input::KeyState::press
        || ws->key_state(input::Keyboard::right) == input::KeyState::press
    ) {
        pos += move_speed * slow_factor * delta_time * right;
    }
    if (ws->key_state(input::Keyboard::q) == input::KeyState::press) {
        pos -= move_speed * slow_factor * delta_time * up;
    }
    if (ws->key_state(input::Keyboard::e) == input::KeyState::press) {
        pos += move_speed * slow_factor * delta_time * up;
    }

    camera->position = pos;
    camera->front_dir = front;
    camera->up_dir = up;
}

}
