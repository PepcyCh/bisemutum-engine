#include <bisemutum/scene_basic/camera_system.hpp>

#include <unordered_set>

#include <bisemutum/scene_basic/camera.hpp>
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/runtime/ecs.hpp>
#include <bisemutum/runtime/scene_object.hpp>
#include <bisemutum/window/window.hpp>

namespace bi {

namespace {

auto copy_camera_data(Ref<gfx::Camera> camera, Ref<rt::SceneObject> object, CameraComponent const& component) -> void {
    auto& transform = object->world_transform();
    camera->position = transform.transform_position({0.0f, 0.0f, 0.0f});
    camera->front_dir = transform.transform_direction({0.0f, 0.0f, -1.0f});
    camera->up_dir = transform.transform_direction({0.0f, 1.0f, 0.0f});
    camera->yfov = component.yfov;
    camera->near_z = component.near_z;
    camera->far_z = component.far_z;
    camera->projection_type = component.projection_type;
    camera->recreate_target_texture(
        component.render_target_size.x, component.render_target_size.y, component.render_target_format
    );
}

}

struct CameraSystem::Impl final {
    Impl() {
        g_engine->ecs_manager()->ecs_registry().on_construct<CameraComponent>().connect<&Impl::on_construct>(this);
        g_engine->ecs_manager()->ecs_registry().on_destroy<CameraComponent>().connect<&Impl::on_destroy>(this);
        g_engine->ecs_manager()->ecs_registry().on_update<CameraComponent>().connect<&Impl::on_update>(this);

        window_resize = g_engine->window()->register_resize_callback(
            [this](Window const& window, WindowSize frame_size, WindowSize logic_size) {
                for (auto entity : window_cameras) {
                    g_engine->ecs_manager()->ecs_registry().patch<CameraComponent>(
                        entity,
                        [logic_size](CameraComponent& component) {
                            component.render_target_size = {logic_size.width, logic_size.height};
                        }
                    );
                }
            }
        );
    }

    auto update() -> void {
        for (auto entity : destroyed_cameras) {
            auto handle_it = camera_handles.find(entity);
            g_engine->graphics_manager()->remove_camera(handle_it->second);
            camera_handles.erase(handle_it);
            if (auto it = dirty_cameras.find(entity); it != dirty_cameras.end()) {
                dirty_cameras.erase(it);
            }
        }
        destroyed_cameras.clear();

        for (auto entity : dirty_cameras) {
            auto object = g_engine->ecs_manager()->scene_object_of(entity);
            auto handle = camera_handles.at(entity);
            auto camera = g_engine->graphics_manager()->get_camera(handle);
            auto& component = g_engine->ecs_manager()->ecs_registry().get<CameraComponent>(entity);

            auto window_camera_it = window_cameras.find(entity);
            if (
                component.render_target_size_mode == CameraRenderTargetSizeMode::window
                && window_camera_it == window_cameras.end()
            ) {
                window_cameras.insert(entity);
                component.render_target_size = {
                    g_engine->window()->logic_size().width,
                    g_engine->window()->logic_size().height,
                };
            } else if (
                component.render_target_size_mode != CameraRenderTargetSizeMode::window
                && window_camera_it != window_cameras.end()
            ) {
                window_cameras.erase(window_camera_it);
            }

            copy_camera_data(camera, object, component);
        }
        dirty_cameras.clear();
    }

    auto on_construct(entt::registry& ecs_registry, entt::entity entity) -> void {
        auto handle = g_engine->graphics_manager()->add_camera();
        camera_handles.insert({entity, handle});
        dirty_cameras.insert(entity);
    }
    auto on_destroy(entt::registry& ecs_registry, entt::entity entity) -> void {
        destroyed_cameras.insert(entity);
    }
    auto on_update(entt::registry& ecs_registry, entt::entity entity) -> void {
        dirty_cameras.insert(entity);
    }

    auto camera_handle_of(entt::entity entity) const -> gfx::CameraHandle {
        if (auto it = camera_handles.find(entity); it != camera_handles.end()) {
            return it->second;
        } else {
            return gfx::CameraHandle::invalid;
        }
    }

    std::unordered_map<entt::entity, gfx::CameraHandle> camera_handles;

    std::unordered_set<entt::entity> dirty_cameras;
    std::unordered_set<entt::entity> destroyed_cameras;

    Window::ResizeCallbackHandle window_resize;
    std::unordered_set<entt::entity> window_cameras;
};

CameraSystem::CameraSystem() = default;
CameraSystem::~CameraSystem() = default;

CameraSystem::CameraSystem(CameraSystem&& rhs) noexcept = default;
auto CameraSystem::operator=(CameraSystem&& rhs) noexcept -> CameraSystem& = default;

auto CameraSystem::update() -> void {
    impl()->update();
}

auto CameraSystem::camera_handle_of(entt::entity entity) const -> gfx::CameraHandle {
    return impl()->camera_handle_of(entity);
}

}
