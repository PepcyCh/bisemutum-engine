#pragma once

#include <chrono>

namespace bi::rt {

struct FrameTimer final {
    auto delta_time() -> double;
    auto total_time() -> double;

    auto reset() -> void;
    auto tick() -> void;
    auto start() -> void;
    auto stop() -> void;

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> base_time_;
    std::chrono::time_point<std::chrono::high_resolution_clock> prev_time_;
    std::chrono::time_point<std::chrono::high_resolution_clock> stop_time_;

    double delta_time_;
    double paused_time_;

    bool stopped_ = true;
};

}
