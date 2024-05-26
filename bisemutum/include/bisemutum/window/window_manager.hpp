#pragma once

#include <string_view>

#include "window_size.hpp"
#include "../math/math.hpp"
#include "../utils/raii_handle.hpp"
#include "../prelude/idiom.hpp"

namespace bi {

struct ImGuiWindowConfig final {
    bool* p_open = nullptr;
    float padding = 8.0f;
};

struct ImGuiWindowArgs final {
    uint2 window_pos;
    uint2 window_size;
};

struct WindowManager final : PImpl<WindowManager> {
    struct Impl;

    WindowManager();
    ~WindowManager();

    auto new_frame() -> void;

    auto frame_count() const -> uint64_t;

    auto frame_size(std::string_view name) const -> WindowSize;
    auto logic_size(std::string_view name) const -> WindowSize;

    auto main_viewport_name() const -> std::string_view;

    auto imgui_window(
        std::string_view name, std::function<auto(ImGuiWindowArgs const&) -> void> func,
        ImGuiWindowConfig const& config = {}
    ) -> void;

    // auto (WindowSize frame_size, WindowSize logic_size) -> void
    using ResizeCallback = std::function<auto(WindowManager const&, WindowSize, WindowSize) -> void>;
    using ResizeCallbackHandle = RaiiHandle<WindowManager, size_t>;
    auto register_resize_callback(std::string_view name, ResizeCallback&& callback) -> ResizeCallbackHandle;
    auto register_resize_callback(ResizeCallback&& callback) -> ResizeCallbackHandle;
};

}
