#include <bisemutum/runtime/prefab.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/system_manager.hpp>
#include <bisemutum/runtime/prefab_manager.hpp>
#include <bisemutum/runtime/component_manager.hpp>
#include <bisemutum/runtime/asset_manager.hpp>
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

    return prefab;
}

auto Prefab::create_from(CRef<SceneObject> object, std::string_view dst_path) -> void {
    auto prefab_mgr = g_engine->system_manager()->get_global_system<PrefabManager>();

    Prefab prefab{};
    prefab.object_ = prefab_mgr->scene()->create_scene_object(nullptr, false);

    prefab.object_->set_name(object->get_name());
    object->for_each_component([&prefab](std::string_view component_type_name, void const* value) {
        auto& clone_func = g_engine->component_manager()->get_metadata(component_type_name).clone_to;
        clone_func(prefab.object_.value(), value);
    });

    g_engine->asset_manager()->create_asset(dst_path, std::move(prefab));
}

auto Prefab::instantiate() -> void {
    auto current_scene = g_engine->world()->current_scene().value();
    auto object = current_scene->create_scene_object(nullptr, false);
    object->set_name(object_->get_name());
    object_->for_each_component([object](std::string_view component_type_name, void* value) {
        auto& clone_func = g_engine->component_manager()->get_metadata(component_type_name).clone_to;
        clone_func(object, value);
    });
}

}
