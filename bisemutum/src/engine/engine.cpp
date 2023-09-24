#include <bisemutum/engine/engine.hpp>

#include <fstream>

#include <bisemutum/window/window.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/rhi/device.hpp>
#include <bisemutum/runtime/world.hpp>
#include <bisemutum/runtime/frame_timer.hpp>
#include <bisemutum/runtime/module.hpp>
#include <bisemutum/runtime/ecs.hpp>
#include <bisemutum/runtime/vfs.hpp>
#include <bisemutum/runtime/logger.hpp>
#include <bisemutum/runtime/component_manager.hpp>
#include <bisemutum/runtime/asset_manager.hpp>
#include <bisemutum/platform/exe_dir.hpp>

#include "register.hpp"
#include "ui.hpp"

namespace bi {

namespace {

auto read_all_from_file(std::ifstream& fin) -> std::string {
    fin.seekg(0, std::ios::end);
    auto length = fin.tellg();
    fin.seekg(0, std::ios::beg);
    std::string res(length, '\0');
    fin.read(res.data(), length);
    while (!res.empty() && res.back() == '\0') { res.pop_back(); }
    return res;
}

struct ProjectSettings final {
    gfx::GraphicsSettings graphics;
};
BI_SREFL(type(ProjectSettings), field(graphics));

struct ProjectInfo final {
    static constexpr std::string_view type_name = "";

    std::string name;
    std::string scene_file;
    std::string renderer;
    ProjectSettings settings;
};
BI_SREFL(type(ProjectInfo), field(name), field(scene_file), field(renderer), field(settings));

}

Engine* g_engine = nullptr;

struct Engine::Impl final {
    Impl() : window(1600, 900, "Bisemutum Engine") {
        register_loggers(logger_manager);
        register_components(component_manager);
        register_assets(asset_manager);
        register_renderers(graphics_manager);
    }

    auto initialize(int argc, char** argv) -> bool {
        // `register_systems` needs engine to be fully constructed
        register_systems(ecs_manager);

        if (!mount_engine_path()) { return false; }

        if (argc < 2) {
            log::critical("general", "Project file is not specified.");
            return false;
        }
        auto project_path = argv[1];
        auto project_info_opt = read_project_file(project_path);
        if (!project_info_opt) { return false; }
        auto& project_info = project_info_opt.value();
        file_system.mount(
            "/project/",
            rt::PhysicalSubFileSystem{std::filesystem::path{project_path}.parent_path()}
        );

        auto graphics_backend_str = magic_enum::enum_name(project_info.settings.graphics.backend);
        graphics_manager.initialize(project_info.settings.graphics);
        graphics_manager.device()->initialize_pipeline_cache_from(
            fmt::format("/project/binaries/gfx-{}/pipeline_cache", graphics_backend_str)
        );
        graphics_manager.set_renderer(project_info.renderer);

        ui = create_empty_ui();
        graphics_manager.set_displayer(ui.displayer());

        if (!module_manager.initialize()) { return false; }

        auto scene_file_opt = file_system.get_file(project_info.scene_file);
        if (!scene_file_opt) {
            log::critical("general", "Scene file '{}' not founc.", project_info.scene_file);
            return false;
        }
        if (!world.load_scene(scene_file_opt.value().read_string_data())) { return false; }

        return true;
    }
    auto mount_engine_path() -> bool {
        auto exe_dir = get_executable_directory();
        auto engine_path_dir = exe_dir / "bisemutum_path.txt";
        std::string engine_path{};
        {
            std::ifstream fin(engine_path_dir);
            if (!fin) {
                log::critical("general", "File bisemutum_path.txt not found.");
                return false;
            }
            engine_path = read_all_from_file(fin);
        }
        if (!std::filesystem::exists(engine_path)) {
            log::critical("general", "File bisemutum_path.txt is broken.");
            return false;
        }
        file_system.mount("/bisemutum/", rt::PhysicalSubFileSystem{engine_path, false});
        return true;
    }
    auto read_project_file(char const* project_file_path) -> Option<ProjectInfo> {
        std::string project_file_str{};
        {
            std::ifstream fin(project_file_path);
            if (!fin) {
                log::critical("general", "Project file not found.");
                return {};
            }
            project_file_str = read_all_from_file(fin);
        }

        try {
            auto value = serde::Value::from_toml(project_file_str);
            return value.get<ProjectInfo>();
        } catch (std::exception const& e) {
            log::critical("general", "Project file is invalid: {}", e.what());
            return {};
        }
    }

    auto finalize() -> bool {
        return module_manager.finalize();
    }

    auto execute() -> void {
        frame_timer.reset();
        window.main_loop([this]() {
            frame_timer.tick();
            ecs_manager.tick();
            graphics_manager.render_frame();
            ui.execute();
            world.do_destroy_scene_objects();
        });
    }

    rt::LoggerManager logger_manager;

    Window window;
    Dyn<IEngineUI>::Box ui;

    rt::FrameTimer frame_timer;
    rt::ModuleManager module_manager;
    rt::FileSystem file_system;

    gfx::GraphicsManager graphics_manager;

    rt::EcsManager ecs_manager;
    rt::ComponentManager component_manager;
    rt::AssetManager asset_manager;
    rt::World world;
};

Engine::Engine() = default;

Engine::~Engine() = default;

auto Engine::initialize(int argc, char** argv) -> bool { return impl()->initialize(argc, argv); }
auto Engine::finalize() -> bool { return impl()->finalize(); }

auto Engine::execute() -> void {
    impl()->execute();
}

auto Engine::window() -> Ref<Window> {
    return impl()->window;
}

auto Engine::graphics_manager() -> Ref<gfx::GraphicsManager> {
    return impl()->graphics_manager;
}

auto Engine::world() -> Ref<rt::World> {
    return impl()->world;
}
auto Engine::frame_timer() -> Ref<rt::FrameTimer> {
    return impl()->frame_timer;
}
auto Engine::module_manager() -> Ref<rt::ModuleManager> {
    return impl()->module_manager;
}
auto Engine::ecs_manager() -> Ref<rt::EcsManager> {
    return impl()->ecs_manager;
}
auto Engine::file_system() -> Ref<rt::FileSystem> {
    return impl()->file_system;
}
auto Engine::logger_manager() -> Ref<rt::LoggerManager> {
    return impl()->logger_manager;
}
auto Engine::component_manager() -> Ref<rt::ComponentManager> {
    return impl()->component_manager;
}
auto Engine::asset_manager() -> Ref<rt::AssetManager> {
    return impl()->asset_manager;
}


auto initialize_engine(int argc, char** argv) -> bool {
    if (g_engine) { return true; }

    g_engine = new Engine{};
    if (g_engine->initialize(argc, argv)) {
        return true;
    }
    delete g_engine;
    g_engine = nullptr;
    return false;
}
auto finalize_engine() -> bool {
    if (!g_engine) { return true; }
    auto ret = g_engine->finalize();
    delete g_engine;
    return ret;
}

}
