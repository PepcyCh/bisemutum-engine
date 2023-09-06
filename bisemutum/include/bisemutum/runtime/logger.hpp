#pragma once

#include "logger_manager.hpp"
#include "../engine/engine.hpp"

namespace bi::log {

#define DEFINE_LOG_METHOD(level) \
    template <typename... Args> \
    auto level(std::string_view logger, spdlog::format_string_t<Args...> fmt, Args&&... args) -> void { \
        g_engine->logger_manager()->level(logger, fmt, std::forward<Args>(args)...); \
    }

DEFINE_LOG_METHOD(trace)
DEFINE_LOG_METHOD(debug)
DEFINE_LOG_METHOD(info)
DEFINE_LOG_METHOD(warn)
DEFINE_LOG_METHOD(error)
DEFINE_LOG_METHOD(critical)

#undef DEFINE_LOG_METHOD

#define BI_ASSERT(expr) do { \
        if (!(expr)) { \
            ::bi::log::critical("assert", "assertion failed in file '{}' line {}, expression: '{}'", \
                __FILE__, __LINE__, #expr); \
        } \
    } while (false)

#define BI_ASSERT_MSG(expr, msg) do { \
        if (!(expr)) { \
            ::bi::log::critical("assert", "{} (file '{}' line {})", msg, __FILE__, __LINE__); \
        } \
    } while (false)

}
