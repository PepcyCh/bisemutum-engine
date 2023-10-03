#pragma once

#include <bisemutum/graphics/handles.hpp>
#include <bisemutum/graphics/resource.hpp>
#include <bisemutum/graphics/camera.hpp>
#include <bisemutum/rhi/command.hpp>
#include <bisemutum/window/window_manager.hpp>

namespace bi {

struct EditorDisplayer final {
    EditorDisplayer();

    auto display(Ref<rhi::CommandEncoder> cmd_encoder, Ref<gfx::Texture> target_texture) -> void;

    auto is_valid() const -> bool;

private:
    auto init_editor_camera() -> void;

    auto move_editor_camera(Ref<gfx::Camera> camera, float delta_time) -> void;

    gfx::CameraHandle editor_camera_ = gfx::CameraHandle::invalid;
    gfx::CameraHandle scene_camera_ = gfx::CameraHandle::invalid;
    bool display_scene_camera_ = false;

    bool camera_turning = false;
    float2 camera_turning_from;

    WindowManager::ResizeCallbackHandle editor_camera_resize_;
};

}
