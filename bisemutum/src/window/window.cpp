#include <bisemutum/window/window.hpp>

#include <iostream>
#include <format>

#include <glfw/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <glfw/glfw3native.h>
#include <bisemutum/containers/slotmap.hpp>

namespace bi {

namespace {

void glfw_error_callback(int error_code, const char *desc) {
    std::cerr << std::format("[glfw error] (error code {}) {}\n", error_code, desc);
}

}

void glfw_mouse_callback(GLFWwindow* glfw_window, int button, int action, int mods);
void glfw_key_callback(GLFWwindow* glfw_window, int key, int scancode, int action, int mods);
void glfw_resize_callback(GLFWwindow* glfw_window, int width, int height);

struct Window::Impl final {
    ~Impl() {
        // ImGui_ImplOpenGL3_Shutdown();
        // ImGui_ImplGlfw_Shutdown();
        // ImGui::DestroyContext();

        glfwDestroyWindow(window);
        glfwTerminate();
    }

    auto init(Window* api_window, uint32_t width, uint32_t height, std::string_view title) -> void {
        this->api_window = api_window;
        logic_size = {width, height};

        glfwSetErrorCallback(glfw_error_callback);

        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
        if (width == 0 || height == 0) {
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        }

        auto owned_title = std::string{title};
        window = glfwCreateWindow(width, height, owned_title.c_str(), nullptr, nullptr);
        if (window == nullptr) {
            return;
        }

        glfwSetWindowUserPointer(window, this);
        glfwSetMouseButtonCallback(window, glfw_mouse_callback);
        glfwSetKeyCallback(window, glfw_key_callback);
        glfwSetFramebufferSizeCallback(window, glfw_resize_callback);

        int frame_width, frame_height;
        glfwGetFramebufferSize(window, &frame_width, &frame_height);
        frame_size.width = frame_width;
        frame_size.height = frame_height;

        float width_scale, height_scale;
        glfwGetWindowContentScale(window, &width_scale, &height_scale);

        // IMGUI_CHECKVERSION();
        // ImGui::CreateContext();
        // ImGuiIO &io = ImGui::GetIO();

        // auto dpi_scale = std::max(width_scale, height_scale);
        // ImFontConfig font_cfg {};
        // font_cfg.SizePixels = kImguiFontSize * dpi_scale;
        // imgui_curr_font_ = io.Fonts->AddFontDefault(&font_cfg);
        // imgui_fonts_[dpi_scale] = imgui_curr_font_;

        // ImGui::StyleColorsClassic();
        // imgui_last_scale_ = dpi_scale;
        // ImGui::GetStyle().ScaleAllSizes(dpi_scale);

        // ImGui_ImplGlfw_InitForOpenGL(window_, true);
        // ImGui_ImplOpenGL3_Init("#version 330");
    }

    auto platform_handle() const -> PlatformWindowHandle {
#ifdef _WIN32
        return PlatformWindowHandle{
            .win32_window = glfwGetWin32Window(window)
        };
#endif
    }

    auto main_loop(std::function<auto() -> void> const& func) -> void {
        frame_count = 0;
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            func();

            ++frame_count;
        }
    }

    auto register_mouse_callback(Window& self, MouseCallback&& callback) -> MouseCallbackHandle {
        return MouseCallbackHandle{&self, mouse_callbacks.insert(std::move(callback))};
    }
    auto unregister_mouse_callback(size_t index) -> void {
        mouse_callbacks.remove(index);
    }

    auto register_key_callback(Window& self, KeyCallback&& callback) -> KeyCallbackHandle {
        return KeyCallbackHandle{&self, key_callbacks.insert(std::move(callback))};
    }
    auto unregister_key_callback(size_t index) -> void {
        key_callbacks.remove(index);
    }

    auto register_resize_callback(Window& self, ResizeCallback&& callback) -> ResizeCallbackHandle {
        return ResizeCallbackHandle{&self, resize_callbacks.insert(std::move(callback))};
    }
    auto unregister_resize_callback(size_t index) -> void {
        resize_callbacks.remove(index);
    }

    Window* api_window = nullptr;

    WindowSize frame_size;
    WindowSize logic_size;

    GLFWwindow *window = nullptr;

    uint64_t frame_count = 0;

    float last_mouse_x = 0.0f;
    float last_mouse_y = 0.0f;
    SlotMap<MouseCallback> mouse_callbacks;

    SlotMap<KeyCallback> key_callbacks;

    SlotMap<ResizeCallback> resize_callbacks;
};

void glfw_mouse_callback(GLFWwindow* glfw_window, int button, int action, int mods) {
    auto window = reinterpret_cast<Window::Impl *>(glfwGetWindowUserPointer(glfw_window));
    auto mouse = button == GLFW_MOUSE_BUTTON_LEFT ? input::Mouse::left
        : button == GLFW_MOUSE_BUTTON_RIGHT ? input::Mouse::right
        : input::Mouse::middle;
    auto state = action == GLFW_PRESS ? input::KeyState::press
        : action == GLFW_RELEASE ? input::KeyState::release
        : input::KeyState::repeat;
    double xpos, ypos;
    glfwGetCursorPos(glfw_window, &xpos, &ypos);
    for (auto const& func : window->mouse_callbacks) {
        func(*window->api_window, mouse, state, xpos, ypos);
    }
}

void glfw_key_callback(GLFWwindow *glfw_window, int key, int scancode, int action, int mods) {
    auto window = reinterpret_cast<Window::Impl *>(glfwGetWindowUserPointer(glfw_window));
    auto state = action == GLFW_PRESS ? input::KeyState::press
        : action == GLFW_RELEASE ? input::KeyState::release
        : input::KeyState::repeat;
    for (auto const& func : window->key_callbacks) {
        func(*window->api_window, static_cast<input::Keyboard>(key), state);
    }
}

void glfw_resize_callback(GLFWwindow *glfw_window, int width, int height) {
    auto window = reinterpret_cast<Window::Impl *>(glfwGetWindowUserPointer(glfw_window));

    float width_scale, height_scale;
    glfwGetWindowContentScale(glfw_window, &width_scale, &height_scale);

    WindowSize frame_size{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    WindowSize logic_size{static_cast<uint32_t>(width / width_scale), static_cast<uint32_t>(height / height_scale)};
    if (window->frame_size != frame_size) {
        window->frame_size = frame_size;
        window->logic_size = logic_size;
        for (auto const& func : window->resize_callbacks) {
            func(*window->api_window, frame_size, logic_size);
        }
    }
}

Window::Window(uint32_t width, uint32_t height, std::string_view title) {
    impl()->init(this, width, height, title);
}

Window::~Window() = default;

auto Window::frame_size() const -> WindowSize { return impl()->frame_size; }
auto Window::logic_size() const -> WindowSize { return impl()->logic_size; }

auto Window::dpi_scale() const -> float {
    return std::max(
        static_cast<float>(impl()->frame_size.width) / impl()->logic_size.width,
        static_cast<float>(impl()->frame_size.height) / impl()->logic_size.height
    );
}

auto Window::platform_handle() const -> PlatformWindowHandle {
    return impl()->platform_handle();
}

auto Window::frame_count() const -> uint64_t {
    return impl()->frame_count;
}

auto Window::main_loop(const std::function<void()> &func) -> void {
    impl()->main_loop(func);
}

auto Window::raw_glfw_window() const -> GLFWwindow* {
    return impl()->window;
}

auto Window::register_mouse_callback(MouseCallback&& callback) -> MouseCallbackHandle {
    return impl()->register_mouse_callback(*this, std::move(callback));
}
auto Window::unregister_mouse_callback(size_t index) -> void {
    impl()->unregister_mouse_callback(index);
}

auto Window::register_key_callback(KeyCallback&& callback) -> KeyCallbackHandle {
    return impl()->register_key_callback(*this, std::move(callback));
}
auto Window::unregister_key_callback(size_t index) -> void {
    impl()->unregister_key_callback(index);
}

auto Window::register_resize_callback(ResizeCallback&& callback) -> ResizeCallbackHandle {
    return impl()->register_resize_callback(*this, std::move(callback));
}
auto Window::unregister_resize_callback(size_t index) -> void {
    impl()->unregister_resize_callback(index);
}

}
