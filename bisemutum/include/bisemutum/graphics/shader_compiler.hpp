#pragma once

#include "shader_compilation_environment.hpp"
#include "../prelude/idiom.hpp"
#include "../prelude/expected.hpp"
#include "../rhi/shader.hpp"

namespace bi::rhi {

struct Device;

}

namespace bi::gfx {

struct ShaderCompiler final : PImpl<ShaderCompiler> {
    struct Impl;

    ShaderCompiler();
    ~ShaderCompiler();

    auto initialize(Ref<rhi::Device> device) -> void;

    auto compile_shader(
        std::string_view source_path,
        std::string_view entry,
        rhi::ShaderStage shader_stage,
        ShaderCompilationEnvironment const& environment
    ) -> Expected<Ref<rhi::ShaderModule>, std::string>;
};

}
