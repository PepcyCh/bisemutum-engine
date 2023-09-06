#include <bisemutum/runtime/logger_manager.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace bi::rt {

namespace {

auto to_spd_log_level(LogLevel log_level) -> spdlog::level::level_enum {
    return static_cast<spdlog::level::level_enum>(static_cast<uint8_t>(log_level));
}

}

LoggerManager::LoggerManager() {
    auto sink_console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    sink_console->set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] [thread %t] %v%$");

    auto sink_file = std::make_shared<spdlog::sinks::basic_file_sink_mt>("Bismuth.log", true);
    sink_file->set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] [thread %t] %v%$");

    sinks_ = {sink_console, sink_file};

    register_logger("general", LogLevel::info);
    register_logger("assert", LogLevel::error);
}

auto LoggerManager::register_logger(std::string_view name, LogLevel level) -> Logger {
    if (auto it = logger_map_.find(name); it != logger_map_.end()) {
        return it->second;
    }

    auto logger = std::make_shared<spdlog::logger>(std::string{name}, sinks_.begin(), sinks_.end());
    logger->set_level(to_spd_log_level(level));

    spdlog::register_logger(logger);
    loggers_.emplace_back(logger);

    return static_cast<Logger>(loggers_.size() - 1);
}

}
