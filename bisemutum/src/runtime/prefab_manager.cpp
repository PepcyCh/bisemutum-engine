#include <bisemutum/runtime/prefab_manager.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/world.hpp>

namespace bi::rt {

struct PrefabManager::Impl final {
    Impl() : scene(g_engine->world()->create_scene(true)) {}

    Ref<Scene> scene;
};

PrefabManager::PrefabManager() = default;

auto PrefabManager::update() -> void {}

auto PrefabManager::scene() const -> Ref<rt::Scene> {
    return impl()->scene;
}

}
