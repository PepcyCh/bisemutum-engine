#include <bisemutum/runtime/prefab.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/system_manager.hpp>
#include <bisemutum/runtime/prefab_manager.hpp>
#include <bisemutum/runtime/component_manager.hpp>
#include <bisemutum/runtime/world.hpp>
#include <bisemutum/runtime/scene.hpp>

namespace bi::rt {

auto Prefab::load(Dyn<rt::IFile>::Ref file) -> rt::AssetAny {
    auto prefab_mgr = g_engine->system_manager()->get_global_system<PrefabManager>();

    Prefab prefab{};

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

    return prefab;
}

auto Prefab::instantiate() -> void {
    auto current_scene = g_engine->world()->current_scene().value();
    auto object = current_scene->create_scene_object(nullptr, false);
    object_->for_each_component([object](std::string_view component_type_name, void* value) {
        auto& clone_func = g_engine->component_manager()->get_metadata(component_type_name).clone_to;
        clone_func(object, value);
    });
}

}
