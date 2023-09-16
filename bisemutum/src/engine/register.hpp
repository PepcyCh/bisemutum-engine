#pragma once

#include <bisemutum/runtime/runtime_forward.hpp>
#include <bisemutum/prelude/ref.hpp>

namespace bi {

auto register_loggers(Ref<rt::LoggerManager> mgr) -> void;

auto register_components(Ref<rt::ComponentManager> mgr) -> void;

auto register_systems(Ref<rt::EcsManager> mgr) -> void;

auto register_assets(Ref<rt::AssetManager> mgr) -> void;

}
