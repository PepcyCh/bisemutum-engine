#pragma once

#include "../prelude/idiom.hpp"
#include "../prelude/ref.hpp"
#include "../runtime/runtime_forward.hpp"

namespace bi {

struct Window;
struct WindowManager;
struct ImGuiRenderer;

namespace gfx {

struct GraphicsManager;

}

namespace drefl {

struct ReflectionManager;

}

namespace editor {

struct MenuManager;

}

struct Engine final : PImpl<Engine> {
    struct Impl;

    Engine();
    ~Engine();

    auto initialize(int argc, char** argv) -> bool;
    auto finalize() -> bool;

    auto execute() -> void;

    auto is_editor_mode() const -> bool;

    auto window() -> Ref<Window>;
    auto window_manager() -> Ref<WindowManager>;
    auto imgui_renderer() -> Ref<ImGuiRenderer>;

    auto graphics_manager() -> Ref<gfx::GraphicsManager>;

    auto world() -> Ref<rt::World>;
    auto frame_timer() -> Ref<rt::FrameTimer>;
    auto module_manager() -> Ref<rt::ModuleManager>;
    auto system_manager() -> Ref<rt::SystemManager>;
    auto file_system() -> Ref<rt::FileSystem>;
    auto logger_manager() -> Ref<rt::LoggerManager>;
    auto component_manager() -> Ref<rt::ComponentManager>;
    auto asset_manager() -> Ref<rt::AssetManager>;

    auto reflection_manager() -> Ref<drefl::ReflectionManager>;

    auto menu_manager() -> Ref<editor::MenuManager>;
};

extern Engine* g_engine;

auto initialize_engine(int argc, char **argv) -> bool;
auto finalize_engine() -> bool;

}
