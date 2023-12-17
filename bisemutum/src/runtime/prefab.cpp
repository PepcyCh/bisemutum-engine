#include <bisemutum/runtime/prefab.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/system_manager.hpp>
#include <bisemutum/runtime/prefab_manager.hpp>
#include <bisemutum/runtime/component_manager.hpp>
#include <bisemutum/runtime/asset_manager.hpp>
#include <bisemutum/runtime/logger.hpp>
#include <bisemutum/runtime/world.hpp>
#include <bisemutum/runtime/scene.hpp>

namespace bi::rt {

auto Prefab::load(Dyn<rt::IFile>::Ref file) -> rt::AssetAny {
    auto prefab_mgr = g_engine->system_manager()->get_global_system<PrefabManager>();

    Prefab prefab{};
    prefab.object_ = prefab_mgr->scene()->create_scene_object(nullptr, false);

    auto value = serde::Value::from_toml(file.read_string_data());
    auto table = value.get_ref<serde::Value::Table>();

    if (auto it = table.find("components"); it != table.end()) {
        for (auto& component_value : it->second.get_ref<serde::Value::Array>()) {
            auto deserializer = g_engine->component_manager()->get_deserializer(
                component_value["type"].get_ref<serde::Value::String>()
            );
            deserializer(prefab.object_.value(), component_value["value"]);
        }
    }
    if (auto it = table.find("name"); it != table.end()) {
        prefab.object_.value()->set_name(it->second.get_ref<serde::Value::String>());
    }

    if (auto it = table.find("objects"); it != table.end()) {
        auto& scene_objects_value = it->second.get_ref<serde::Value::Array>();
        std::vector<Ref<SceneObject>> parsed_objects{prefab.object_.value()};
        parsed_objects.reserve(scene_objects_value.size() + 1);
        std::vector<int> objects_parent;
        objects_parent.reserve(scene_objects_value.size());
        for (auto& object_value : scene_objects_value) {
            auto& object_table = object_value.get_ref<serde::Value::Table>();
            auto parent = object_table.at("parent").get<int>();
            objects_parent.push_back(parent);
            parsed_objects.push_back(prefab_mgr->scene()->create_scene_object(prefab.object_.value(), false));
            if (auto it = object_table.find("components"); it != object_table.end()) {
                for (auto& component_value : it->second.get_ref<serde::Value::Array>()) {
                    auto deserializer = g_engine->component_manager()->get_deserializer(
                        component_value["type"].get_ref<serde::Value::String>()
                    );
                    deserializer(parsed_objects.back(), component_value["value"]);
                }
            }
            if (auto it = object_table.find("name"); it != object_table.end()) {
                parsed_objects.back()->set_name(it->second.get_ref<serde::Value::String>());
            }
        }
        for (size_t i = 0; i < parsed_objects.size(); i++) {
            parsed_objects[i]->attach_under(parsed_objects[objects_parent[i]]);
        }
    }

    return prefab;
}

auto Prefab::create_from(CRef<SceneObject> object, std::string_view dst_path) -> void {
    auto prefab_mgr = g_engine->system_manager()->get_global_system<PrefabManager>();

    Prefab prefab{};
    prefab.object_ = object->clone(true, prefab_mgr->scene());

    g_engine->asset_manager()->create_asset(dst_path, std::move(prefab));
}

auto Prefab::save(Dyn<rt::IFile>::Ref file) const -> void {
    serde::Value value{};
    value["name"] = std::string{object_->get_name()};
    auto& components_value = value["components"];
    object_->for_each_component([&components_value](std::string_view component_type, void const* component_value) {
        auto serializer = g_engine->component_manager()->get_serializer(component_type);
        serde::Value value{};
        serializer(value["value"], component_value);
        value["type"] = serde::Value::String{component_type};
        components_value.push_back(std::move(value));
    });

    auto& objects_value = value["objects"];
    std::vector<std::pair<CRef<SceneObject>, size_t>> stack{{object_.value(), 0}};
    while (!stack.empty()) {
        auto [u, index] = stack.back();
        stack.pop_back();
        u->for_each_children([&objects_value, &stack, index](CRef<SceneObject> ch) {
            serde::Value object_value{};
            object_value["name"] = std::string{ch->get_name()};
            object_value["parent"] = static_cast<serde::Value::Integer>(index);
            auto& components_value = object_value["components"];
            ch->for_each_component([&components_value](std::string_view component_type, void const* component_value) {
                auto serializer = g_engine->component_manager()->get_serializer(component_type);
                serde::Value value{};
                serializer(value["value"], component_value);
                value["type"] = serde::Value::String{component_type};
                components_value.push_back(std::move(value));
            });
            objects_value.push_back(std::move(object_value));
            stack.emplace_back(ch, objects_value.size());
        });
    }

    file.write_string_data(value.to_toml());
}

auto Prefab::instantiate() -> void {
    auto current_scene = g_engine->world()->current_scene().value();
    object_->clone(true, current_scene);
}

}
