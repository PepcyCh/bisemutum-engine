#include <bisemutum/window/window_manager.hpp>

#include <imgui.h>
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/window/window.hpp>
#include <bisemutum/containers/hash.hpp>
#include <bisemutum/containers/slotmap.hpp>

namespace bi {

struct WindowManager::Impl final {
    auto new_frame(WindowManager& window_mgr) -> void {
        for (auto& [_, wd] : windows_data) {
            if (wd.on_resize) {
                wd.on_resize = false;
                for (auto& callback : wd.resize_callbacks) {
                    callback(window_mgr, wd.frame_size, wd.logic_size);
                }
            }
        }

        auto& main_wd = windows_data[""];
        auto ws = g_engine->window();
        if (main_wd.frame_size != ws->frame_size()) {
            main_wd.frame_size = ws->frame_size();
            main_wd.logic_size = ws->logic_size();
            main_wd.enabled = true;
            for (auto& callback : main_wd.resize_callbacks) {
                callback(window_mgr, main_wd.frame_size, main_wd.logic_size);
            }
        }
    }

    auto frame_size(std::string_view name) const -> WindowSize {
        if (auto it = windows_data.find(name); it != windows_data.end()) {
            return it->second.frame_size;
        } else {
            return {};
        }
    }
    auto logic_size(std::string_view name) const -> WindowSize {
        if (auto it = windows_data.find(name); it != windows_data.end()) {
            return it->second.logic_size;
        } else {
            return {};
        }
    }

    auto imgui_window(
        std::string_view name, std::function<auto(ImGuiWindowArgs const&) -> void>&& func,
        ImGuiWindowConfig const& config
    ) -> void {
        auto wd_it = windows_data.find(name);
        if (wd_it == windows_data.end()) {
            wd_it = windows_data.insert({std::string{name}, {}}).first;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {config.padding, config.padding});
        auto enabled = ImGui::Begin(wd_it->first.c_str(), config.p_open);
        auto window_pos = ImGui::GetWindowPos();
        auto window_size = ImGui::GetWindowSize();
        if (enabled) {
            func(ImGuiWindowArgs{
                .window_size = {window_size.x, window_size.y},
            });
        }
        ImGui::End();
        ImGui::PopStyleVar();

        auto frame_size = WindowSize{static_cast<uint32_t>(window_size.x), static_cast<uint32_t>(window_size.y)};
        auto dpi_scale = ImGui::GetWindowDpiScale();
        auto logic_size = WindowSize{
            static_cast<uint32_t>(window_size.x / dpi_scale),
            static_cast<uint32_t>(window_size.y / dpi_scale),
        };
        if (wd_it->second.frame_size != frame_size) {
            wd_it->second.on_resize = true;
            wd_it->second.frame_size = frame_size;
            wd_it->second.logic_size = logic_size;
        }
        if (wd_it->second.enabled != enabled) {
            wd_it->second.enabled = enabled;
        }
    }

    auto register_resize_callback(std::string_view name, ResizeCallback&& callback) -> ResizeCallbackHandle {
        auto wd_it = windows_data.find(name);
        if (wd_it == windows_data.end()) {
            wd_it = windows_data.insert({std::string{name}, {}}).first;
        }

        return ResizeCallbackHandle{
            wd_it->second.resize_callbacks.insert(std::move(callback)),
            [this, wd_it](size_t index) {
                wd_it->second.resize_callbacks.remove(index);
            },
        };
    }

    struct WindowData final {
        WindowSize frame_size;
        WindowSize logic_size;
        bool enabled = false;

        bool on_resize = false;
        SlotMap<ResizeCallback> resize_callbacks;
    };
    StringHashMap<WindowData> windows_data;
};

WindowManager::WindowManager() = default;
WindowManager::~WindowManager() = default;

auto WindowManager::new_frame() -> void {
    impl()->new_frame(*this);
}

auto WindowManager::frame_size(std::string_view name) const -> WindowSize {
    return impl()->frame_size(name);
}
auto WindowManager::logic_size(std::string_view name) const -> WindowSize {
    return impl()->logic_size(name);
}

auto WindowManager::main_viewport_name() const -> std::string_view {
    return g_engine->is_editor_mode() ? "Viewport" : "";
}

auto WindowManager::imgui_window(
    std::string_view name, std::function<auto(ImGuiWindowArgs const&) -> void> func, ImGuiWindowConfig const& config
) -> void {
    impl()->imgui_window(name, std::move(func), config);
}

auto WindowManager::register_resize_callback(std::string_view name, ResizeCallback&& callback) -> ResizeCallbackHandle {
    return impl()->register_resize_callback(name, std::move(callback));
}
auto WindowManager::register_resize_callback(ResizeCallback&& callback) -> ResizeCallbackHandle {
    return impl()->register_resize_callback("", std::move(callback));
}

}
