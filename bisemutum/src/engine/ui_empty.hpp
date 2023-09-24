#pragma once

#include <unordered_set>

#include <bisemutum/graphics/displayer.hpp>
#include <bisemutum/graphics/handles.hpp>
#include <entt/entity/registry.hpp>

namespace bi {

struct EmptyEngineUI final {
    EmptyEngineUI();

    auto displayer() -> Dyn<gfx::IDisplayer>::Ref { return displayer_; }

    auto execute() -> void;

private:
    auto on_construct(entt::registry& ecs_registry, entt::entity entity) -> void;
    auto on_destroy(entt::registry& ecs_registry, entt::entity entity) -> void;

    gfx::BlitDisplayer displayer_;

    gfx::CameraHandle displayed_camera_ = gfx::CameraHandle::invalid;
    entt::entity displayed_camera_entity_ = static_cast<entt::entity>(-1);
    std::unordered_set<entt::entity> camera_entities_{};
};

}
