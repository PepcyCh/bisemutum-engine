#pragma once

#include <functional>
#include <string_view>

#include "input.hpp"
#include "../prelude/bitflags.hpp"
#include "../prelude/idiom.hpp"
#include "../platform/window.hpp"

struct GLFWwindow;

namespace bi {

struct WindowSize final {
    uint32_t width = 0;
    uint32_t height = 0;

    auto operator==(WindowSize const& rhs) const -> bool = default;
};

struct Window final : PImpl<Window> {
    struct Impl;

    Window(uint32_t width, uint32_t height, std::string_view title);
    ~Window();

    auto frame_size() const -> WindowSize;
    auto logic_size() const -> WindowSize;
    auto platform_handle() const -> PlatformWindowHandle;

    auto frame_count() const -> uint64_t;

    auto main_loop(std::function<auto() -> void> const& func) -> void;

    template <typename Callback, auto unregister_func>
    struct CallbackHandle final : MoveOnly {
        CallbackHandle() noexcept = default;
        ~CallbackHandle() noexcept { reset(); }

        CallbackHandle(CallbackHandle&& rhs) noexcept {
            window_ = rhs.window_;
            index_ = rhs.index_;
            rhs.window_ = nullptr;
        }
        auto operator=(CallbackHandle&& rhs) noexcept -> CallbackHandle& {
            reset();
            window_ = rhs.window_;
            index_ = rhs.index_;
            rhs.window_ = nullptr;
            return *this;
        }

        auto reset() noexcept -> void {
            if (window_) {
                (window_->*unregister_func)(index_);
                window_ = nullptr;
            }
        }

    private:
        friend Window::Impl;
        CallbackHandle(Window* window, size_t index) : window_(window), index_(index) {}
        Window* window_ = nullptr;
        size_t index_;
    };

private:
    template <typename Callback, auto unregister_func>
    friend struct CallbackHandle;
    auto unregister_mouse_callback(size_t index) -> void;
    auto unregister_key_callback(size_t index) -> void;
    auto unregister_resize_callback(size_t index) -> void;

public:
    using MouseCallback = std::function<auto(input::Mouse, input::KeyState, float, float) -> void>;
    using MouseCallbackHandle = CallbackHandle<MouseCallback, &Window::unregister_mouse_callback>;
    auto register_mouse_callback(MouseCallback&& callback) -> MouseCallbackHandle;

    using KeyCallback = std::function<auto(input::Keyboard, input::KeyState) -> void>;
    using KeyCallbackHandle = CallbackHandle<KeyCallback, &Window::unregister_key_callback>;
    auto register_key_callback(KeyCallback&& callback) -> KeyCallbackHandle;

    // auto (WindowSize frame_size, WindowSize logic_size) -> void
    using ResizeCallback = std::function<auto(WindowSize, WindowSize) -> void>;
    using ResizeCallbackHandle = CallbackHandle<ResizeCallback, &Window::unregister_resize_callback>;
    auto register_resize_callback(ResizeCallback&& callback) -> ResizeCallbackHandle;
};

}
