#include <bisemutum/runtime/world.hpp>

#include <list>
#include <unordered_map>

#include <bisemutum/runtime/scene.hpp>
#include <bisemutum/runtime/scene_object.hpp>
#include <bisemutum/runtime/logger.hpp>
#include <bisemutum/runtime/component_manager.hpp>
#include <bisemutum/utils/serde.hpp>

namespace bi::rt {

struct World::Impl final {
    auto create_scene() -> Ref<Scene> {
        auto& scene = scenes.emplace_front();
        scenes_it_map.insert({&scene, scenes.begin()});
        if (!current_scene) {
            current_scene = &scene;
        }
        return scene;
    }
    auto destroy_scene(Ref<Scene> scene) -> void {
        if (current_scene == scene) {
            current_scene = nullptr;
        }
        scene->for_each_root_object([this](Ref<SceneObject> object) {
            destroy_scene_object_and_its_children(object);
        });
        auto it = scenes_it_map.find(scene.get());
        scenes.erase(it->second);
        scenes_it_map.erase(it);
    }

    auto create_scene_object(Ref<Scene> scene, Ptr<SceneObject> parent) -> Ref<SceneObject> {
        auto& object = scene_objects.emplace_front(scene);
        scene_objects_it_map.insert({&object, scene_objects.begin()});
        if (parent.has_value()) {
            scene->add_child_object(object, parent.value());
        } else {
            scene->add_root_object(object);
        }
        return object;
    }

    auto destroy_scene_object(Ref<SceneObject> object) -> void {
        object->extract_all_children();
        object->remove_self_from_sibling_list(false);
        object->mark_as_destroyed();
        destroyed_objects.push_back(object);
    }

    auto destroy_scene_object_and_its_children(Ref<SceneObject> object) -> void {
        object->remove_self_from_sibling_list(false);
        object->mark_as_destroyed();
        destroyed_objects.push_back(object);
        std::function<auto(Ref<SceneObject>) -> void> destroy_all_children =
            [this, &destroy_all_children](Ref<SceneObject> ch) {
                ch->mark_as_destroyed();
                destroyed_objects.push_back(ch);
                ch->for_each_children(destroy_all_children);
            };
        object->for_each_children(destroy_all_children);
    }

    auto do_destroy_scene_objects() -> void {
        for (auto object : destroyed_objects) {
            auto it = scene_objects_it_map.find(object.get());
            scene_objects.erase(it->second);
            scene_objects_it_map.erase(it);
        }
        destroyed_objects.clear();
    }

    auto load_scene(std::string_view scene_file_str) -> bool {
        auto scene = create_scene();
        try {
            auto scene_value = serde::Value::from_toml(scene_file_str);
            auto& scene_objects_value = scene_value["objects"].get_ref<serde::Value::Array>();
            std::vector<Ref<SceneObject>> parsed_objects;
            parsed_objects.reserve(scene_objects_value.size());
            std::vector<int> objects_parent;
            objects_parent.reserve(scene_objects_value.size());
            for (auto& object_value : scene_objects_value) {
                auto& object_table = object_value.get_ref<serde::Value::Table>();
                int parent = -1;
                if (auto it = object_table.find("parent"); it != object_table.end()) {
                    parent = it->second.get<int>();
                }
                objects_parent.push_back(parent);
                parsed_objects.push_back(create_scene_object(scene, nullptr));
                if (auto it = object_table.find("components"); it != object_table.end()) {
                    for (auto& component_value : it->second.get_ref<serde::Value::Array>()) {
                        auto deserializer = g_engine->component_manager()->get_deserializer(
                            component_value["type"].get_ref<serde::Value::String>()
                        );
                        deserializer(parsed_objects.back(), component_value["value"]);
                    }
                }
            }
            for (size_t i = 0; i < parsed_objects.size(); i++) {
                if (objects_parent[i] >= 0 && objects_parent[i] < parsed_objects.size()) {
                    parsed_objects[i]->attach_under(parsed_objects[objects_parent[i]]);
                }
            }
            return true;
        } catch (std::exception const& e) {
            destroy_scene(scene);
            log::error("general", "Scene file is invalid: {}", e.what());
            return false;
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

World::~World() = default;

auto World::current_scene() -> Ptr<Scene> {
    return impl()->current_scene;
}
auto World::current_scene() const -> CPtr<Scene> {
    return impl()->current_scene;
}

auto World::create_scene() -> Ref<Scene> {
    return impl()->create_scene();
}

auto World::create_scene_object(Ref<Scene> scene, Ptr<SceneObject> parent) -> Ref<SceneObject> {
    return impl()->create_scene_object(scene, parent);
}
auto World::destroy_scene_object(Ref<SceneObject> object) -> void {
    impl()->destroy_scene_object(object);
}
auto World::destroy_scene_object_and_its_children(Ref<SceneObject> object) -> void {
    impl()->destroy_scene_object_and_its_children(object);
}
auto World::do_destroy_scene_objects() -> void {
    impl()->do_destroy_scene_objects();
}

auto World::load_scene(std::string_view scene_json_str) -> bool {
    return impl()->load_scene(scene_json_str);
}

}
