#include <bisemutum/runtime/prefab.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/component_manager.hpp>

namespace bi::rt {

auto Prefab::load(Dyn<rt::IFile>::Ref file) -> rt::AssetAny {
    Prefab prefab{};

    auto value = serde::Value::from_toml(file.read_string_data());
    auto table = value.get_ref<serde::Value::Table>();

    if (auto it = table.find("components"); it != table.end()) {
        for (auto& component_value : it->second.get_ref<serde::Value::Array>()) {
            auto deserializer = g_engine->component_manager()->get_deserializer(
                component_value["type"].get_ref<serde::Value::String>()
            );
            prefab.components_.push_back(std::move(deserializer));
        }
    }
    if (auto it = table.find("name"); it != table.end()) {
        prefab.name_ = it->second.get_ref<serde::Value::String>();
    }

    return prefab;
}

}
