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
                auto it = scene_systems_it->second.try_emplace(type, creator()).first;
                it->second.init_on(scene);
            }
        }
    }
    auto tick_update() -> void {
        for (auto& [_, system] : global_systems) {
            system.update();
        }

        auto& scene_systems = systems.at(g_engine->world()->current_scene().value());
        for (auto& [_, system] : scene_systems) {
            system.update();
        }
    }
    auto tick_post_update() -> void {
        for (auto& [_, system] : global_systems) {
            system.post_update();
        }

        auto& scene_systems = systems.at(g_engine->world()->current_scene().value());
        for (auto& [_, system] : scene_systems) {
            system.post_update();
        }
    }

    auto register_global_system(std::type_index type, GlobalSystemCreator&& creator) -> void {
        global_systems.try_emplace(type, creator());
    }
    auto get_global_system(std::type_index type) -> Dyn<IGlobalSystem>::Ptr {
        if (auto it = global_systems.find(type); it != global_systems.end()) {
            return &it->second;
        } else {
            return nullptr;
        }
    }

    auto register_system(std::type_index type, SystemCreator&& creator) -> void {
        system_creators.insert({type, std::move(creator)});
    }
    auto get_system_for(Ref<Scene> scene, std::type_index type) -> Dyn<ISystem>::Ptr {
        auto& scene_systems = systems.at(scene);
        if (auto it = scene_systems.find(type); it != scene_systems.end()) {
            return &it->second;
        } else {
            return nullptr;
        }
    }

    std::unordered_map<std::type_index, Dyn<IGlobalSystem>::Box> global_systems;
    std::unordered_map<std::type_index, SystemCreator> system_creators;
    std::unordered_map<Ref<Scene>, std::unordered_map<std::type_index, Dyn<ISystem>::Box>> systems;
};

SystemManager::SystemManager() = default;

auto SystemManager::init_on(Ref<Scene> scene) -> void {
    impl()->init_on(scene);
}
auto SystemManager::tick_update() -> void {
    impl()->tick_update();
}
auto SystemManager::tick_post_update() -> void {
    impl()->tick_post_update();
}

auto SystemManager::register_global_system(std::type_index type, GlobalSystemCreator creator) -> void {
    impl()->register_global_system(type, std::move(creator));
}
auto SystemManager::get_global_system(std::type_index type) -> Dyn<IGlobalSystem>::Ptr {
    return impl()->get_global_system(type);
}

auto SystemManager::register_system(std::type_index type, SystemCreator creator) -> void {
    impl()->register_system(type, std::move(creator));
}

auto SystemManager::get_system_for_current_scene(std::type_index type) -> Dyn<ISystem>::Ptr {
    return impl()->get_system_for(g_engine->world()->current_scene().value(), type);
}
auto SystemManager::get_system_for(Ref<Scene> scene, std::type_index type) -> Dyn<ISystem>::Ptr {
    return impl()->get_system_for(scene, type);
}

}
