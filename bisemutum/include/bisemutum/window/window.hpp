#pragma once

#include <functional>
#include <string_view>

#include "input.hpp"
#include "window_size.hpp"
#include "../math/math.hpp"
#include "../utils/raii_handle.hpp"
#include "../prelude/bitflags.hpp"
#include "../prelude/idiom.hpp"
#include "../platform/window.hpp"

struct GLFWwindow;

namespace bi {

struct Window final : PImpl<Window> {
    struct Impl;

    Window(uint32_t width, uint32_t height, std::string_view title);

    auto frame_size() const -> WindowSize;
    auto logic_size() const -> WindowSize;
    auto dpi_scale() const -> float;

    auto platform_handle() const -> PlatformWindowHandle;

    auto frame_count() const -> uint64_t;

    auto main_loop(std::function<auto() -> void> const& func) -> void;

    auto raw_glfw_window() const -> GLFWwindow*;

    using MouseCallback = std::function<auto(Window const&, input::Mouse, input::KeyState, float, float) -> void>;
    using MouseCallbackHandle = RaiiHandle<Window, size_t>;
    auto register_mouse_callback(MouseCallback&& callback) -> MouseCallbackHandle;

    using KeyCallback = std::function<auto(Window const&, input::Keyboard, input::KeyState) -> void>;
    using KeyCallbackHandle = RaiiHandle<Window, size_t>;
    auto register_key_callback(KeyCallback&& callback) -> KeyCallbackHandle;

    // auto (WindowSize frame_size, WindowSize logic_size) -> void
    using ResizeCallback = std::function<auto(Window const&, WindowSize, WindowSize) -> void>;
    using ResizeCallbackHandle = RaiiHandle<Window, size_t>;
    auto register_resize_callback(ResizeCallback&& callback) -> ResizeCallbackHandle;

    auto key_state(input::Keyboard key) const -> input::KeyState;
    auto mouse_state(input::Mouse mouse) const -> input::KeyState;
    auto cursor_pos() const -> float2;
};

}
