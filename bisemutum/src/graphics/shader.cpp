#include <bisemutum/graphics/shader.hpp>

namespace bi::gfx {

auto FragmentShader::modify_compiler_environment(ShaderCompilationEnvironment& compilation_environment) const -> void {
    switch (needed_vertex_attributes) {
        case VertexAttributesType::full:
            compilation_environment.set_define("VERTEX_ATTRIBUTES_TYPE_FULL");
            break;
        case VertexAttributesType::position_only:
            compilation_environment.set_define("VERTEX_ATTRIBUTES_TYPE_POSITION_ONLY");
            break;
    }

    if (modify_compiler_environment_func) {
        modify_compiler_environment_func(compilation_environment);
    }
}

}
