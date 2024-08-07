#pragma once

#include <functional>

#include "handles.hpp"
#include "shader_param.hpp"
#include "../prelude/idiom.hpp"
#include "../runtime/scene.hpp"

namespace bi::gfx {

struct Camera;
struct Drawable;
struct DrawableShaderData;
struct RaytracingPassContext;

struct GpuSceneSystem final : PImpl<GpuSceneSystem> {
    struct Impl;

    GpuSceneSystem();

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
    auto get_drawable(DrawableHandle handle) const -> CRef<Drawable>;

    auto drawables_hash() -> size_t;

    auto num_drawables() const -> size_t;
    auto get_all_drawables() -> std::vector<Ref<Drawable>>;

    auto for_each_drawable(std::function<auto(Drawable&) -> void> func) -> void;
    auto for_each_drawable(std::function<auto(Drawable const&) -> void> func) const -> void;

    auto for_each_drawable_with_shader_data(
        std::function<auto(Drawable&, DrawableShaderData const& drawable_data) -> void> func
    ) -> void;
    auto for_each_drawable_with_shader_data(
        std::function<auto(Drawable const&, DrawableShaderData const& drawable_data) -> void> func
    ) const -> void;

    auto drawable_data_of(DrawableHandle handle) const -> DrawableShaderData;
    auto drawable_data_of(Drawable const& drawable) const -> DrawableShaderData;

    auto drawable_continuous_index_of(DrawableHandle handle) const -> size_t;
    auto drawable_continuous_index_of(Drawable const& drawable) const -> size_t;

private:
    friend GraphicsManager;
    friend RaytracingPassContext;
    auto shader_params() -> ShaderParameter&;
    auto shader_params_metadata() const -> ShaderParameterMetadataList const&;
    auto update_shader_params() -> void;
};

}
