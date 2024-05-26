#pragma once

#include <bisemutum/graphics/handles.hpp>
#include <bisemutum/graphics/resource.hpp>
#include <bisemutum/graphics/camera.hpp>
#include <bisemutum/rhi/command.hpp>
#include <bisemutum/runtime/scene_object.hpp>
#include <bisemutum/window/window_manager.hpp>
#include <bisemutum/editor/file_dialog.hpp>

namespace bi {

struct EditorDisplayer final {
    auto display(Ref<rhi::CommandEncoder> cmd_encoder, Ref<gfx::Texture> target_texture) -> void;

    auto is_valid() const -> bool;

private:
    auto init_displayer() -> void;

    auto init_editor_camera() -> void;

    auto move_editor_camera(Ref<gfx::Camera> camera, float delta_time) -> void;

    gfx::CameraHandle editor_camera_ = gfx::CameraHandle::invalid;
    gfx::CameraHandle scene_camera_ = gfx::CameraHandle::invalid;
    bool display_scene_camera_ = false;

    bool camera_turning_ = false;
    float2 camera_turning_from_;

    Ptr<rt::SceneObject> selected_object_ = nullptr;

    WindowManager::ResizeCallbackHandle editor_camera_resize_;

    editor::FileDialog file_dialog_;

    int mani_gizmo_op_ = 0;
    int mani_gizmo_mode_ = 0;
};

}
