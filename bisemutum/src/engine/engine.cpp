#include <bisemutum/engine/engine.hpp>

#include <fstream>

#include <bisemutum/window/window.hpp>
#include <bisemutum/window/window_manager.hpp>
#include <bisemutum/window/imgui_renderer.hpp>
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
#include <bisemutum/utils/drefl.hpp>
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

struct ExecutableOptions final {
    char const* project_file = nullptr;
    bool editor = false;
};

auto parse_options(int argc, char** argv) -> ExecutableOptions {
    ExecutableOptions opt{};

    if (argc < 2) { return opt; }
    opt.project_file = argv[1];

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-editor") == 0) {
            opt.editor = true;
        } else {
            log::warn("general", "Unknown comman line option '{}'", argv[i]);
        }
    }

    return opt;
}

} // namespace

Engine* g_engine = nullptr;

struct Engine::Impl final {
    Impl() : window(1600, 900, "Bisemutum Engine") {
        register_loggers(logger_manager);
    }

    auto initialize(int argc, char** argv) -> bool {
        if (!mount_engine_path()) { return false; }

        auto opt = parse_options(argc, argv);
        if (!opt.project_file) {
            log::critical("general", "Project file is not specified.");
            return false;
        }
        is_editor_mode = opt.editor;

        do_register();

        auto project_info_opt = read_project_file(opt.project_file);
        if (!project_info_opt) { return false; }
        auto& project_info = project_info_opt.value();
        file_system.mount(
            "/project/",
            rt::PhysicalSubFileSystem{std::filesystem::path{opt.project_file}.parent_path()}
        );

        auto graphics_backend_str = magic_enum::enum_name(project_info.settings.graphics.backend);
        graphics_manager.initialize(project_info.settings.graphics);
        graphics_manager.device()->initialize_pipeline_cache_from(
            fmt::format("/project/binaries/gfx-{}/pipeline_cache", graphics_backend_str)
        );
        graphics_manager.set_renderer(project_info.renderer);

        imgui_renderer.initialize(window, graphics_manager);

        if (opt.editor) {
            ui = create_editor_ui();
        } else {
            ui = create_empty_ui();
        }
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
    auto do_register() -> void {
        register_components(component_manager);
        register_assets(asset_manager);
        register_systems(ecs_manager);
        register_renderers(graphics_manager);
        register_reflections(reflection_manager);
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
        graphics_manager.wait_idle();
        imgui_renderer.finalize();
        return module_manager.finalize();
    }

    auto execute() -> void {
        frame_timer.reset();
        window.main_loop([this]() {
            window_manager.new_frame();
            graphics_manager.new_frame();
            frame_timer.tick();
            ecs_manager.tick();
            graphics_manager.render_frame();
            ui.execute();
            world.do_destroy_scene_objects();
        });
    }

    rt::LoggerManager logger_manager;

    Window window;
    WindowManager window_manager;
    Dyn<IEngineUI>::Box ui;

    rt::FrameTimer frame_timer;
    rt::ModuleManager module_manager;
    rt::FileSystem file_system;

    gfx::GraphicsManager graphics_manager;
    ImGuiRenderer imgui_renderer;

    rt::EcsManager ecs_manager;
    rt::ComponentManager component_manager;
    rt::AssetManager asset_manager;
    rt::World world;

    drefl::ReflectionManager reflection_manager;

    bool is_editor_mode = false;
};

Engine::Engine() = default;

Engine::~Engine() = default;

auto Engine::initialize(int argc, char** argv) -> bool { return impl()->initialize(argc, argv); }
auto Engine::finalize() -> bool { return impl()->finalize(); }

auto Engine::execute() -> void {
    impl()->execute();
}

auto Engine::is_editor_mode() const -> bool {
    return impl()->is_editor_mode;
}

auto Engine::window() -> Ref<Window> {
    return impl()->window;
}
auto Engine::window_manager() -> Ref<WindowManager> {
    return impl()->window_manager;
}
auto Engine::imgui_renderer() -> Ref<ImGuiRenderer> {
    return impl()->imgui_renderer;
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
auto Engine::reflection_manager() -> Ref<drefl::ReflectionManager> {
    return impl()->reflection_manager;
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
