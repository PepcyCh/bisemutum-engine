#include <bisemutum/runtime/ecs.hpp>

namespace bi::rt {

struct EcsManager::Impl final {
    auto tick() -> void {
        for (auto& [_, system] : systems) {
            system.update();
        }
    }

    auto register_system(std::type_index type, Dyn<ISystem>::Box&& system) -> void {
        systems.insert({type, std::move(system)});
    }

    entt::registry ecs_registry;
    std::unordered_map<std::type_index, Dyn<ISystem>::Box> systems;
};

EcsManager::EcsManager() = default;

EcsManager::~EcsManager() = default;

auto EcsManager::get_ecs_registry() -> entt::registry& {
    return impl()->ecs_registry;
}

auto EcsManager::get_ecs_registry() const -> entt::registry const& {
    return impl()->ecs_registry;
}

auto EcsManager::tick() -> void {
    impl()->tick();
}

auto EcsManager::register_system(std::type_index type, Dyn<ISystem>::Box system) -> void {
    impl()->register_system(type, std::move(system));
}

}
