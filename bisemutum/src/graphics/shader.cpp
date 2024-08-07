#include <bisemutum/graphics/shader.hpp>

namespace bi::gfx {

auto FragmentShader::modify_compiler_environment(ShaderCompilationEnvironment& compilation_environment) const -> void {
    compilation_environment.set_define(
        "VERTEX_ATTRIBUTES_OUT",
        std::to_string(needed_vertex_attributes.raw_value())
    );

    if (modify_compiler_environment_func) {
        modify_compiler_environment_func(compilation_environment);
    }
}

auto ComputeShader::modify_compiler_environment(ShaderCompilationEnvironment& compilation_environment) const -> void {
    if (modify_compiler_environment_func) {
        modify_compiler_environment_func(compilation_environment);
    }
}

auto RaytracingShaders::modify_compiler_environment(ShaderCompilationEnvironment& compilation_environment) const -> void {
    if (modify_compiler_environment_func) {
        modify_compiler_environment_func(compilation_environment);
    }
}

}
