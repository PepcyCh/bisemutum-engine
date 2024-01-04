#pragma once

#include "component.hpp"
#include "../engine/engine.hpp"
#include "../runtime/world.hpp"
#include "../runtime/scene.hpp"
#include "../runtime/scene_object.hpp"

namespace bi::rt {

template <typename T>
concept TVolumeComponent = requires (const T v) {
    TComponent<T>;
    { v.global } -> std::same_as<bool const&>;
    { v.priority } -> std::same_as<float const&>;
};

template <TVolumeComponent Component>
auto find_volume_component_for(Ref<Scene> scene, float3 position) -> Component const* {
    auto view = scene->ecs_registry().view<Component>();
    Component const* volume = nullptr;
    for (auto entity : view) {
        auto& data = view.template get<Component>(entity);
        auto inside = data.global;
        if (!data.global) {
            auto object = scene->object_of(entity);
            auto& transform = object->world_transform();
            auto p = transform.inverse().transform_position(position);
            inside = math::all(math::greaterThanEqual(p, float3(-1.0f)) & math::lessThanEqual(p, float3(1.0f)));
        }
        if (inside && (!volume || data.priority > volume->priority)) {
            volume = &data;
        }
    }
    return volume;
}
template <TVolumeComponent Component>
auto find_volume_component_for(float3 position) -> Component const* {
    auto scene = g_engine->world()->current_scene().value();
    return find_volume_component_for<Component>(scene, position);
}

}
