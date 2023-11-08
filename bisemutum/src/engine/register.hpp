#pragma once

#include <bisemutum/runtime/runtime_forward.hpp>
#include <bisemutum/prelude/ref.hpp>

namespace bi {

auto register_loggers(Ref<rt::LoggerManager> mgr) -> void;

auto register_components(Ref<rt::ComponentManager> mgr) -> void;

auto register_systems(Ref<rt::SystemManager> mgr) -> void;

auto register_assets(Ref<rt::AssetManager> mgr) -> void;

namespace drefl {

struct ReflectionManager;

}

auto register_reflections(Ref<drefl::ReflectionManager> mgr) -> void;

namespace gfx {

struct GraphicsManager;

}

auto register_renderers(Ref<gfx::GraphicsManager> mgr) -> void;

namespace editor {

struct MenuManager;

}

auto register_menu_actions(Ref<editor::MenuManager> mgr) -> void;

}
