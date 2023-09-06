#include <bisemutum/runtime/frame_timer.hpp>

namespace bi::rt {

auto FrameTimer::delta_time() -> double {
    return delta_time_;
}

auto FrameTimer::total_time() -> double {
    if (stopped_) {
        std::chrono::duration<double> temp = stop_time_ - base_time_;
        return temp.count() - paused_time_;
    } else {
        std::chrono::duration<double> temp = prev_time_ - base_time_;
        return temp.count() - paused_time_;
    }
}

auto FrameTimer::reset() -> void {
    stopped_ = false;
    prev_time_ = base_time_ = std::chrono::high_resolution_clock::now();
    delta_time_ = 0.0;
    paused_time_ = 0.0;
}

auto FrameTimer::tick() -> void {
    auto curr_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> delta = curr_time - prev_time_;
    delta_time_ = delta.count();
    prev_time_ = curr_time;
}

auto FrameTimer::start() -> void {
    if (stopped_) {
        auto curr_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> pause = curr_time - stop_time_;
        paused_time_ += pause.count();
        delta_time_ = 0.0;
        prev_time_ = curr_time;
        stopped_ = false;
    }
}

auto FrameTimer::stop() -> void {
    if (!stopped_) {
        stop_time_ = std::chrono::high_resolution_clock::now();
        delta_time_ = 0.0;
        stopped_ = true;
    }
}

}
