#pragma once

#include <spdlog/spdlog.h>

namespace bi::rt {

enum class LogLevel : uint8_t {
    trace,
    debug,
    info,
    warn,
    error,
    critical,
};

enum class Logger : size_t;

struct LoggerManager final {
    LoggerManager();

    auto register_logger(std::string_view name, LogLevel level = LogLevel::info) -> Logger;

#define DEFINE_LOG_METHOD(level) \
    template <typename... Args> \
    auto level(Logger logger, spdlog::format_string_t<Args...> fmt, Args&&... args) -> void { \
        loggers_[static_cast<size_t>(logger)]->level(fmt, std::forward<Args>(args)...); \
    } \
    template <typename... Args> \
    auto level(std::string_view logger, spdlog::format_string_t<Args...> fmt, Args&&... args) -> void { \
        if (auto it = logger_map_.find(logger); it != logger_map_.end()) { \
            level(it->second, fmt, std::forward<Args>(args)...); \
        } \
    }

    DEFINE_LOG_METHOD(trace)
    DEFINE_LOG_METHOD(debug)
    DEFINE_LOG_METHOD(info)
    DEFINE_LOG_METHOD(warn)
    DEFINE_LOG_METHOD(error)
    DEFINE_LOG_METHOD(critical)

#undef DEFINE_LOG_METHOD

private:
    std::vector<spdlog::sink_ptr> sinks_;
    std::vector<std::shared_ptr<spdlog::logger>> loggers_;
    std::unordered_map<std::string_view, Logger> logger_map_;
};

}
