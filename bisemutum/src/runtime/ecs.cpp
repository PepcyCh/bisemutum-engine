#include <bisemutum/runtime/ecs.hpp>

namespace bi::rt {

struct EcsManager::Impl final {
    auto scene_object_of(entt::entity entity) const -> Ref<SceneObject> {
        return entity_map.at(entity);
    }
    auto add_scene_object(Ref<SceneObject> object, entt::entity entity) -> void {
        entity_map.insert({entity, object});
    }
    auto remove_scene_object(entt::entity entity) -> void {
        if (auto it = entity_map.find(entity); it != entity_map.end()) {
            entity_map.erase(it);
        }
    }

    auto tick() -> void {
        for (auto& [_, system] : systems) {
            system.update();
        }
    }

    auto register_system(std::type_index type, Dyn<ISystem>::Box&& system) -> void {
        systems.insert({type, std::move(system)});
    }
    auto get_system(std::type_index type) const -> Dyn<ISystem>::CPtr {
        if (auto it = systems.find(type); it != systems.end()) {
            return &it->second;
        } else {
            return nullptr;
        }
    }

    entt::registry ecs_registry;
    std::unordered_map<entt::entity, Ref<SceneObject>> entity_map;
    std::unordered_map<std::type_index, Dyn<ISystem>::Box> systems;
};

EcsManager::EcsManager() = default;

EcsManager::~EcsManager() = default;

auto EcsManager::ecs_registry() -> entt::registry& {
    return impl()->ecs_registry;
}

auto EcsManager::ecs_registry() const -> entt::registry const& {
    return impl()->ecs_registry;
}

auto EcsManager::scene_object_of(entt::entity entity) const -> Ref<SceneObject> {
    return impl()->scene_object_of(entity);
}

auto EcsManager::tick() -> void {
    impl()->tick();
}

auto EcsManager::register_system(std::type_index type, Dyn<ISystem>::Box system) -> void {
    impl()->register_system(type, std::move(system));
}
auto EcsManager::get_system(std::type_index type) const -> Dyn<ISystem>::CPtr {
    return impl()->get_system(type);
}

auto EcsManager::add_scene_object(Ref<SceneObject> object, entt::entity entity) -> void {
    impl()->add_scene_object(object, entity);
}
auto EcsManager::remove_scene_object(entt::entity entity) -> void {
    impl()->remove_scene_object(entity);
}

}
