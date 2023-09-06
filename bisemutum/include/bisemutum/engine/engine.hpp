#pragma once

#include "../prelude/idiom.hpp"
#include "../prelude/ref.hpp"
#include "../runtime/runtime_forward.hpp"

namespace bi {

struct Window;

namespace gfx {

struct GraphicsManager;

}

struct Engine final : PImpl<Engine> {
    struct Impl;

    Engine();
    ~Engine();

    auto initialize(int argc, char** argv) -> bool;
    auto finalize() -> bool;

    auto execute() -> void;

    auto window() -> Ref<Window>;

    auto graphics_manager() -> Ref<gfx::GraphicsManager>;

    auto world() -> Ref<rt::World>;
    auto frame_timer() -> Ref<rt::FrameTimer>;
    auto module_manager() -> Ref<rt::ModuleManager>;
    auto ecs_manager() -> Ref<rt::EcsManager>;
    auto file_system() -> Ref<rt::FileSystem>;
    auto logger_manager() -> Ref<rt::LoggerManager>;
    auto component_manager() -> Ref<rt::ComponentManager>;
    auto asset_manager() -> Ref<rt::AssetManager>;
};

extern Engine* g_engine;

auto initialize_engine(int argc, char **argv) -> bool;
auto finalize_engine() -> bool;

}
