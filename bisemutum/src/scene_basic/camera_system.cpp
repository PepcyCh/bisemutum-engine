#include <bisemutum/scene_basic/camera_system.hpp>

#include <unordered_set>

#include <bisemutum/scene_basic/camera.hpp>
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/graphics/gpu_scene_system.hpp>
#include <bisemutum/runtime/system_manager.hpp>
#include <bisemutum/runtime/scene_object.hpp>
#include <bisemutum/window/window_manager.hpp>

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
    auto init_on(Ref<rt::Scene> scene) -> void {
        this->scene = scene;
        scene->ecs_registry().on_construct<CameraComponent>().connect<&Impl::on_construct>(this);
        scene->ecs_registry().on_destroy<CameraComponent>().connect<&Impl::on_destroy>(this);
        scene->ecs_registry().on_update<CameraComponent>().connect<&Impl::on_update>(this);

        window_resize = g_engine->window_manager()->register_resize_callback(
            g_engine->window_manager()->main_viewport_name(),
            [this](WindowManager const& window, WindowSize frame_size, WindowSize logic_size) {
                if (!window_cameras.empty()) {
                    g_engine->graphics_manager()->wait_idle();
                }
                for (auto entity : window_cameras) {
                    this->scene->ecs_registry().patch<CameraComponent>(
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
        auto gpu_scene = g_engine->system_manager()->get_system_for<gfx::GpuSceneSystem>(scene.value());

        for (auto entity : destroyed_cameras) {
            auto handle_it = camera_handles.find(entity);
            gpu_scene->remove_camera(handle_it->second);
            camera_handles.erase(handle_it);
            if (auto it = dirty_cameras.find(entity); it != dirty_cameras.end()) {
                dirty_cameras.erase(it);
            }
        }
        destroyed_cameras.clear();

        auto wm = g_engine->window_manager();
        for (auto entity : dirty_cameras) {
            auto object = scene->object_of(entity);
            auto handle = camera_handles.at(entity);
            auto camera = gpu_scene->get_camera(handle);
            auto& component = scene->ecs_registry().get<CameraComponent>(entity);

            auto window_camera_it = window_cameras.find(entity);
            if (
                component.render_target_size_mode == CameraRenderTargetSizeMode::window
                && window_camera_it == window_cameras.end()
            ) {
                window_cameras.insert(entity);
                component.render_target_size = {
                    std::max(wm->logic_size(wm->main_viewport_name()).width, 1u),
                    std::max(wm->logic_size(wm->main_viewport_name()).height, 1u),
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

        if (main_camera == gfx::CameraHandle::invalid && !camera_handles.empty()) {
            std::tie(main_camera_entity, main_camera) = *camera_handles.begin();
        }
    }

    auto on_construct(entt::registry& ecs_registry, entt::entity entity) -> void {
        auto gpu_scene = g_engine->system_manager()->get_system_for<gfx::GpuSceneSystem>(scene.value());
        auto handle = gpu_scene->add_camera();
        camera_handles.insert({entity, handle});
        dirty_cameras.insert(entity);

        if (main_camera == gfx::CameraHandle::invalid) {
            main_camera = handle;
            main_camera_entity = entity;
        }
    }
    auto on_destroy(entt::registry& ecs_registry, entt::entity entity) -> void {
        destroyed_cameras.insert(entity);

        if (main_camera_entity == entity) {
            main_camera = gfx::CameraHandle::invalid;
        }
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

    Ptr<rt::Scene> scene;

    std::unordered_map<entt::entity, gfx::CameraHandle> camera_handles;
    gfx::CameraHandle main_camera = gfx::CameraHandle::invalid;
    entt::entity main_camera_entity;

    std::unordered_set<entt::entity> dirty_cameras;
    std::unordered_set<entt::entity> destroyed_cameras;

    WindowManager::ResizeCallbackHandle window_resize;
    std::unordered_set<entt::entity> window_cameras;
};

CameraSystem::CameraSystem() = default;

auto CameraSystem::init_on(Ref<rt::Scene> scene) -> void {
    impl()->init_on(scene);
}
auto CameraSystem::update() -> void {
    impl()->update();
}

auto CameraSystem::main_camera_handle() const -> gfx::CameraHandle {
    return impl()->main_camera;
}

auto CameraSystem::camera_handle_of(entt::entity entity) const -> gfx::CameraHandle {
    return impl()->camera_handle_of(entity);
}

}
