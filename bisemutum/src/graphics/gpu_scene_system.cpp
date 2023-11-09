#include <bisemutum/graphics/gpu_scene_system.hpp>

#include <bisemutum/containers/slotmap.hpp>
#include <bisemutum/graphics/camera.hpp>
#include <bisemutum/graphics/drawable.hpp>

namespace bi::gfx {

struct GpuSceneSystem::Impl final {
    auto init_on(Ref<rt::Scene> scene) -> void {}
    auto update() -> void {}

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

    SlotMap<Camera, CameraHandle> cameras;
    SlotMap<Drawable, DrawableHandle> drawables;
};

GpuSceneSystem::GpuSceneSystem() = default;
GpuSceneSystem::~GpuSceneSystem() = default;

GpuSceneSystem::GpuSceneSystem(GpuSceneSystem&& rhs) noexcept = default;
auto GpuSceneSystem::operator=(GpuSceneSystem&& rhs) noexcept -> GpuSceneSystem& = default;

auto GpuSceneSystem::init_on(Ref<rt::Scene> scene) -> void {
    impl()->init_on(scene);
}
auto GpuSceneSystem::update() -> void {
    impl()->update();
}

auto GpuSceneSystem::add_camera() -> CameraHandle {
    return impl()->add_camera();
}
auto GpuSceneSystem::remove_camera(CameraHandle handle) -> void {
    impl()->remove_camera(handle);
}
auto GpuSceneSystem::get_camera(CameraHandle handle) -> Ref<Camera> {
    return impl()->get_camera(handle);
}
auto GpuSceneSystem::for_each_camera(std::function<auto(Camera&) -> void> func) -> void {
    impl()->for_each_camera(std::move(func));
}
auto GpuSceneSystem::for_each_camera(std::function<auto(Camera const&) -> void> func) const -> void {
    impl()->for_each_camera(std::move(func));
}

auto GpuSceneSystem::add_drawable() -> DrawableHandle {
    return impl()->add_drawable();
}
auto GpuSceneSystem::remove_drawable(DrawableHandle handle) -> void {
    impl()->remove_drawable(handle);
}
auto GpuSceneSystem::get_drawable(DrawableHandle handle) -> Ref<Drawable> {
    return impl()->get_drawable(handle);
}
auto GpuSceneSystem::for_each_drawable(std::function<auto(Drawable&) -> void> func) -> void {
    impl()->for_each_drawable(std::move(func));
}
auto GpuSceneSystem::for_each_drawable(std::function<auto(Drawable const&) -> void> func) const -> void {
    impl()->for_each_drawable(std::move(func));
}

}
