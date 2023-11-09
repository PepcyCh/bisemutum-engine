#include <bisemutum/scene_basic/static_mesh_render_system.hpp>

#include <unordered_set>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/scene_basic/static_mesh.hpp>
#include <bisemutum/scene_basic/mesh_renderer.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/graphics/gpu_scene_system.hpp>
#include <bisemutum/graphics/drawable.hpp>
#include <bisemutum/runtime/system_manager.hpp>
#include <bisemutum/runtime/scene_object.hpp>

namespace bi {

struct StaticMeshRenderSystem::Impl final {
    auto init_on(Ref<rt::Scene> scene) -> void {
        this->scene = scene;
        scene->ecs_registry().on_construct<StaticMeshComponent>().connect<&Impl::on_construct>(this);
        scene->ecs_registry().on_destroy<StaticMeshComponent>().connect<&Impl::on_destroy>(this);
        scene->ecs_registry().on_update<StaticMeshComponent>().connect<&Impl::on_renderer_update>(this);
        scene->ecs_registry().on_construct<MeshRendererComponent>().connect<&Impl::on_construct>(this);
        scene->ecs_registry().on_destroy<MeshRendererComponent>().connect<&Impl::on_destroy>(this);
        scene->ecs_registry().on_update<MeshRendererComponent>().connect<&Impl::on_mesh_update>(this);
    }

    auto update() -> void {
        auto gpu_scene = g_engine->system_manager()->get_system_for<gfx::GpuSceneSystem>(scene.value());

        for (auto entity : destroyed_entities) {
            auto handle_it = drawable_handles.find(entity);
            if (handle_it != drawable_handles.end()) {
                gpu_scene->remove_drawable(handle_it->second);
                drawable_handles.erase(handle_it);
            }
            if (auto it = dirty_entities.find(entity); it != dirty_entities.end()) {
                dirty_entities.erase(it);
            }
        }
        destroyed_entities.clear();

        for (auto entity : dirty_entities) {
            auto object = scene->object_of(entity);
            if (!object->has_components<StaticMeshComponent, MeshRendererComponent>()) {
                continue;
            }
            auto handle_it = drawable_handles.find(entity);
            if (handle_it == drawable_handles.end()) {
                handle_it = drawable_handles.insert({entity, gpu_scene->add_drawable()}).first;
            }
            auto drawable = gpu_scene->get_drawable(handle_it->second);
            auto mesh = object->get_component<StaticMeshComponent>();
            auto renderer = object->get_component<MeshRendererComponent>();
            mesh->static_mesh.load();
            renderer->material.load();
            drawable->mesh = mesh->static_mesh.asset().get();
            drawable->material = &renderer->material.asset()->material;
            if (dirty_meshes.contains(entity)) {
                drawable->shader_params.reset();
            }
            if (!drawable->shader_params) {
                drawable->shader_params.initialize(drawable->mesh->shader_params_metadata());
            }
        }
        dirty_entities.clear();
        dirty_meshes.clear();

        for (auto& [entity, handle] : drawable_handles) {
            auto object = scene->object_of(entity);
            auto drawable = gpu_scene->get_drawable(handle);
            drawable->transform = object->world_transform();
        }
    }

    auto on_construct(entt::registry& ecs_registry, entt::entity entity) -> void {
        dirty_entities.insert(entity);
    }
    auto on_destroy(entt::registry& ecs_registry, entt::entity entity) -> void {
        destroyed_entities.insert(entity);
    }
    auto on_renderer_update(entt::registry& ecs_registry, entt::entity entity) -> void {
        dirty_entities.insert(entity);
    }
    auto on_mesh_update(entt::registry& ecs_registry, entt::entity entity) -> void {
        dirty_entities.insert(entity);
        dirty_meshes.insert(entity);
    }

    Ptr<rt::Scene> scene;

    std::unordered_map<entt::entity, gfx::DrawableHandle> drawable_handles;
    std::unordered_set<entt::entity> dirty_entities;
    std::unordered_set<entt::entity> dirty_meshes;
    std::unordered_set<entt::entity> destroyed_entities;
};

StaticMeshRenderSystem::StaticMeshRenderSystem() = default;
StaticMeshRenderSystem::~StaticMeshRenderSystem() = default;

StaticMeshRenderSystem::StaticMeshRenderSystem(StaticMeshRenderSystem&& rhs) noexcept = default;
auto StaticMeshRenderSystem::operator=(StaticMeshRenderSystem&& rhs) noexcept -> StaticMeshRenderSystem& = default;

auto StaticMeshRenderSystem::init_on(Ref<rt::Scene> scene) -> void {
    impl()->init_on(scene);
}
auto StaticMeshRenderSystem::update() -> void {
    impl()->update();
}

}
