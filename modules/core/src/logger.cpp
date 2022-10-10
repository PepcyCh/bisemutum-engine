#include "core/logger.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

BISMUTH_NAMESPACE_BEGIN

namespace {

spdlog::level::level_enum ToSpdLevel(LogLevel level) {
    return static_cast<spdlog::level::level_enum>(static_cast<uint8_t>(level) + 1);
}

}

LoggerManager &LoggerManager::Get() {
    static LoggerManager logger_mgr {};
    return logger_mgr;
}

LoggerManager::LoggerManager() {
    auto sink_console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    sink_console->set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] [thread %t] %v%$");

    auto sink_file = std::make_shared<spdlog::sinks::basic_file_sink_mt>("Bismuth.log", true);
    sink_file->set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] [thread %t] %v%$");

    sinks_ = { sink_console, sink_file };

    gGeneralLogger = RegisterLogger("General", LogLevel::eInfo);
}

Logger LoggerManager::RegisterLogger(const char *name, LogLevel level) {
    if (auto it = logger_map_.find(name); it != logger_map_.end()) {
        return it->second;
    }

    auto logger = std::make_shared<spdlog::logger>(name, sinks_.begin(), sinks_.end());
    logger->set_level(ToSpdLevel(level));

    spdlog::register_logger(logger);
    loggers_.emplace_back(logger);

    Logger ret {};
    ret.index_ = loggers_.size() - 1;
    return ret;
}

BISMUTH_NAMESPACE_END
