#include <bisemutum/scene_basic/camera_system.hpp>

#include <unordered_set>

#include <bisemutum/scene_basic/camera.hpp>
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/runtime/ecs.hpp>

namespace bi {

namespace {

auto copy_camera_data(Ref<gfx::Camera> camera, CameraComponent const& component) -> void {
    camera->position = component.position;
    camera->front_dir = component.front_dir;
    camera->up_dir = component.up_dir;
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
            auto handle = camera_handles.at(entity);
            auto camera = g_engine->graphics_manager()->get_camera(handle);
            auto const& component = g_engine->ecs_manager()->ecs_registry().get<CameraComponent>(entity);
            copy_camera_data(camera, component);
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

    std::unordered_map<entt::entity, gfx::CameraHandle> camera_handles;
    std::unordered_set<entt::entity> dirty_cameras;
    std::unordered_set<entt::entity> destroyed_cameras;
};

CameraSystem::CameraSystem() = default;
CameraSystem::~CameraSystem() = default;

CameraSystem::CameraSystem(CameraSystem&& rhs) noexcept = default;
auto CameraSystem::operator=(CameraSystem&& rhs) noexcept -> CameraSystem& = default;

auto CameraSystem::update() -> void {
    impl()->update();
}

}
