#pragma once

#include "../containers/hash.hpp"

namespace bi::gfx {

struct ShaderCompilationEnvironment final {
    auto set_define(std::string_view key, std::string const& defined = "1") -> void;
    auto set_define(std::string_view key, std::string&& defined) -> void;
    template <typename T>
    auto set_define(std::string_view key, T const& value) -> void {
        set_define(key, std::to_string(value));
    }

    auto reset_define(std::string_view key) -> void;

    auto set_replace_arg(std::string_view key, std::string const& content) -> void;
    auto set_replace_arg(std::string_view key, std::string&& content) -> void;

    auto reset_replace_arg(std::string_view key) -> void;

    auto get_config_identifier() const -> std::string;

    StringHashMap<std::string> defines{};
    StringHashMap<std::string> replace_args{};
};

}
