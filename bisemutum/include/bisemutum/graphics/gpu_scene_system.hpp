#pragma once

#include <functional>

#include "handles.hpp"
#include "../prelude/idiom.hpp"
#include "../runtime/scene.hpp"

namespace bi::gfx {

struct Camera;
struct Drawable;
struct DrawableShaderData;

struct GpuSceneSystem final : PImpl<GpuSceneSystem> {
    struct Impl;

    GpuSceneSystem();
    ~GpuSceneSystem();

    GpuSceneSystem(GpuSceneSystem&& rhs) noexcept;
    auto operator=(GpuSceneSystem&& rhs) noexcept -> GpuSceneSystem&;

    auto init_on(Ref<rt::Scene> scene) -> void;
    auto update() -> void;
    auto post_update() -> void;

    auto add_camera() -> CameraHandle;
    auto remove_camera(CameraHandle handle) -> void;
    auto get_camera(CameraHandle handle) -> Ref<Camera>;
    auto for_each_camera(std::function<auto(Camera&) -> void> func) -> void;
    auto for_each_camera(std::function<auto(Camera const&) -> void> func) const -> void;

    auto add_drawable() -> DrawableHandle;
    auto remove_drawable(DrawableHandle handle) -> void;
    auto get_drawable(DrawableHandle handle) -> Ref<Drawable>;

    auto for_each_drawable(std::function<auto(Drawable&) -> void> func) -> void;
    auto for_each_drawable(std::function<auto(Drawable const&) -> void> func) const -> void;

    auto for_each_drawable_with_shader_data(
        std::function<auto(Drawable&, DrawableShaderData const& drawable_data) -> void> func
    ) -> void;
    auto for_each_drawable_with_shader_data(
        std::function<auto(Drawable const&, DrawableShaderData const& drawable_data) -> void> func
    ) const -> void;
};

}
