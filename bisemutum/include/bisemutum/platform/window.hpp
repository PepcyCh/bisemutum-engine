#pragma once

#include <cstdint>

namespace bi {

struct PlatformWindowHandle final {
#ifdef _WIN32
    void* win32_window = nullptr;
#else
    uint32_t xcb_window = 0;
    void* xcb_connection = nullptr;
#endif
};

}
