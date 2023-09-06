#include <bisemutum/graphics/shader_compilation_environment.hpp>

#include <fmt/format.h>

namespace bi::gfx {

auto ShaderCompilationEnvironment::set_define(std::string_view key, std::string const& defined) -> void {
    defines.insert({std::string{key}, defined});
}
auto ShaderCompilationEnvironment::set_define(std::string_view key, std::string&& defined) -> void {
    defines.insert({std::string{key}, std::move(defined)});
}
auto ShaderCompilationEnvironment::reset_define(std::string_view key) -> void {
    if (auto it = defines.find(key); it != defines.end()) {
        defines.erase(it);
    }
}

auto ShaderCompilationEnvironment::set_replace_arg(std::string_view key, std::string const& content) -> void {
    replace_args.insert({'$' + std::string{key}, content});
}
auto ShaderCompilationEnvironment::set_replace_arg(std::string_view key, std::string&& content) -> void {
    replace_args.insert({'$' + std::string{key}, std::move(content)});
}
auto ShaderCompilationEnvironment::reset_replace_arg(std::string_view key) -> void {
    if (auto it = replace_args.find('$' + std::string{key}); it != replace_args.end()) {
        replace_args.erase(it);
    }
}

auto ShaderCompilationEnvironment::get_config_identifier() const -> std::string {
    size_t defines_hash = 0;
    for (auto& [key, defined] : defines) {
        defines_hash = hash_combine(defines_hash, hash(key, defined));
    }
    auto defines_hash_hex = fmt::format("{:x}", defines_hash);

    size_t replace_hash = 0;
    for (auto& [key, content] : replace_args) {
        replace_hash = hash_combine(replace_hash, hash(key, content));
    }
    auto replace_hash_hex = fmt::format("{:x}", replace_hash);

    return fmt::format("D{}-R{}", defines_hash_hex, replace_hash_hex);
}

}
