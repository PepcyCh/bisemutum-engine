#include <bisemutum/runtime/world.hpp>

#include <list>
#include <unordered_map>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/scene.hpp>
#include <bisemutum/runtime/scene_object.hpp>
#include <bisemutum/runtime/logger.hpp>
#include <bisemutum/runtime/component_manager.hpp>
#include <bisemutum/runtime/system_manager.hpp>
#include <bisemutum/utils/serde.hpp>

namespace bi::rt {

struct World::Impl final {
    auto create_scene(bool is_dummy_scene) -> Ref<Scene> {
        auto& scene = scenes.emplace_front();
        scenes_it_map.insert({&scene, scenes.begin()});
        if (!current_scene && !is_dummy_scene) {
            current_scene = &scene;
        }
        g_engine->system_manager()->init_on(scene);
        return scene;
    }

    auto destroy_scene(Ref<Scene> scene) -> void {
        if (current_scene == scene) {
            current_scene = nullptr;
        }
        scene->for_each_root_object([scene](Ref<SceneObject> object) {
            scene->destroy_scene_object_and_its_children(object);
        });
        auto it = scenes_it_map.find(scene.get());
        scenes.erase(it->second);
        scenes_it_map.erase(it);
    }

    auto load_scene(std::string_view scene_file_str) -> bool {
        auto scene = create_scene(false);
        try {
            auto scene_value = serde::Value::from_toml(scene_file_str);
            scene->load_from_value(std::move(scene_value));
            return true;
        } catch (std::exception const& e) {
            destroy_scene(scene);
            log::error("general", "Scene file is invalid: {}", e.what());
            return false;
        }
    }

    auto save_currnet_scene(Dyn<IFile>::Ref scene_file) const -> void {
        if (current_scene) {
            serde::Value value{};
            current_scene->save_to_value(value);
            scene_file.write_string_data(value.to_toml());
        }
    }

    std::list<Scene> scenes;
    std::list<SceneObject> scene_objects;

    std::unordered_map<Scene*, std::list<Scene>::iterator> scenes_it_map;
    std::unordered_map<SceneObject*, std::list<SceneObject>::iterator> scene_objects_it_map;

    std::vector<Ref<SceneObject>> destroyed_objects;

    Ptr<Scene> current_scene = nullptr;
};

World::World() = default;

auto World::current_scene() -> Ptr<Scene> {
    return impl()->current_scene;
}
auto World::current_scene() const -> CPtr<Scene> {
    return impl()->current_scene;
}

auto World::create_scene(bool is_dummy_scene) -> Ref<Scene> {
    return impl()->create_scene(is_dummy_scene);
}

auto World::load_scene(std::string_view scene_json_str) -> bool {
    return impl()->load_scene(scene_json_str);
}
auto World::save_currnet_scene(Dyn<IFile>::Ref scene_file) const -> void {
    impl()->save_currnet_scene(scene_file);
}

}
