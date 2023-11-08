#include <bisemutum/runtime/system_manager.hpp>

#include <unordered_map>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/world.hpp>

namespace bi::rt {

struct SystemManager::Impl final {
    auto init_on(Ref<Scene> scene) -> void {
        auto [scene_systems_it, need_to_init] = systems.try_emplace(scene);
        if (need_to_init) {
            scene_systems_it->second.reserve(system_creators.size());
            for (auto& [type, creator] : system_creators) {
                auto it = scene_systems_it->second.insert({type, creator()}).first;
                it->second.init_on(scene);
            }
        }
    }
    auto tick() -> void {
        auto& scene_systems = systems.at(g_engine->world()->current_scene().value());
        for (auto& [_, system] : scene_systems) {
            system.update();
        }
    }

    auto register_system(std::type_index type, SystemCreator&& creator) -> void {
        system_creators.insert({type, std::move(creator)});
    }
    auto get_system(std::type_index type) const -> Dyn<ISystem>::CPtr {
        auto& scene_systems = systems.at(g_engine->world()->current_scene().value());
        if (auto it = scene_systems.find(type); it != scene_systems.end()) {
            return &it->second;
        } else {
            return nullptr;
        }
    }

    std::unordered_map<std::type_index, SystemCreator> system_creators;
    std::unordered_map<Ref<Scene>, std::unordered_map<std::type_index, Dyn<ISystem>::Box>> systems;
};

SystemManager::SystemManager() = default;

SystemManager::~SystemManager() = default;

auto SystemManager::init_on(Ref<Scene> scene) -> void {
    impl()->init_on(scene);
}
auto SystemManager::tick() -> void {
    impl()->tick();
}

auto SystemManager::register_system(std::type_index type, SystemCreator creator) -> void {
    impl()->register_system(type, std::move(creator));
}
auto SystemManager::get_system(std::type_index type) const -> Dyn<ISystem>::CPtr {
    return impl()->get_system(type);
}

}
