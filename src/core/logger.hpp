#pragma once

#include <spdlog/spdlog.h>

#include "log.hpp"

BISMUTH_NAMESPACE_BEGIN

enum class LogLevel : uint8_t {
    eDebug,
    eInfo,
    eWarn,
    eError,
    eCrtical,
};

class LoggerManager {
public:
    static LoggerManager &Get();

    Logger RegisterLogger(const char *name, LogLevel level = LogLevel::eInfo);

    template <typename... Args>
    void Debug(Logger logger, spdlog::format_string_t<Args...> fmt, Args &&... args) {
        loggers_[logger.index_]->debug(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void Info(Logger logger, spdlog::format_string_t<Args...> fmt, Args &&... args) {
        loggers_[logger.index_]->info(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void Warn(Logger logger, spdlog::format_string_t<Args...> fmt, Args &&... args) {
        loggers_[logger.index_]->warn(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void Error(Logger logger, spdlog::format_string_t<Args...> fmt, Args &&... args) {
        loggers_[logger.index_]->error(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void Critical(Logger logger, spdlog::format_string_t<Args...> fmt, Args &&... args) {
        loggers_[logger.index_]->critical(fmt, std::forward<Args>(args)...);
        std::exit(-1);
    }

private:
    LoggerManager();

    std::vector<spdlog::sink_ptr> sinks_;
    std::vector<std::shared_ptr<spdlog::logger>> loggers_;
    std::unordered_map<std::string, Logger> logger_map_;
};

inline Logger gGeneralLogger;

#define BI_DEBUG(logger, fmt, ...) LoggerManager::Get().Debug(logger, fmt, ##__VA_ARGS__)
#define BI_INFO(logger, fmt, ...) LoggerManager::Get().Info(logger, fmt, ##__VA_ARGS__)
#define BI_WARN(logger, fmt, ...) LoggerManager::Get().Warn(logger, fmt, ##__VA_ARGS__)
#define BI_ERROR(logger, fmt, ...) LoggerManager::Get().Error(logger, fmt, ##__VA_ARGS__)
#define BI_CRTICAL(logger, fmt, ...) LoggerManager::Get().Critical(logger, fmt, ##__VA_ARGS__)

#define BI_ASSERT(expr) do { \
    if (!(expr)) { \
        BI_CRTICAL(gGeneralLogger, "assertion failed in file '{}' line {}, expression: '{}'", \
            __FILE__, __LINE__, #expr); \
    } \
} while (false)
#define BI_ASSERT_MSG(expr, msg) do { \
    if (!(expr)) { \
        BI_CRTICAL(gGeneralLogger, "{} (file '{}' line {})", msg, __FILE__, __LINE__); \
    } \
} while (false)

BISMUTH_NAMESPACE_END
