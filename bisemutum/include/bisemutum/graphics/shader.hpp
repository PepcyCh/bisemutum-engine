#pragma once

#include <functional>

#include "vertex_attributes_type.hpp"
#include "shader_param.hpp"
#include "shader_compilation_environment.hpp"
#include "../rhi/pipeline.hpp"

namespace bi::gfx {

struct FragmentShader final {
    template <typename ParamsStruct>
    auto set_shader_params_struct() { shader_params_metadata = shader_parameter_metadata_list_of<ParamsStruct>(); }

    auto modify_compiler_environment(ShaderCompilationEnvironment& compilation_environment) const -> void;

    std::string source_path;
    std::string source_entry;
    ShaderParameterMetadataList shader_params_metadata;
    std::function<auto(ShaderCompilationEnvironment&) -> void> modify_compiler_environment_func;
    VertexAttributesType needed_vertex_attributes = VertexAttributesType::full;

    rhi::FrontFace front_face = rhi::FrontFace::ccw;
    rhi::CullMode cull_mode = rhi::CullMode::none;
    rhi::PolygonMode polygon_mode = rhi::PolygonMode::fill;
    bool conservative_rasterization = false;

    bool depth_write = true;
    bool depth_test = true;
    rhi::CompareOp depth_compare_op = rhi::CompareOp::less;

    bool stencil_test = false;
    rhi::StencilFaceState stencil_front_face;
    rhi::StencilFaceState stencil_back_face;
    uint8_t stencil_compare_mask = 0xff;
    uint8_t stencil_write_mask = 0xff;
    uint8_t stencil_reference = 0;
};

struct ComputeShader final {
    template <typename ParamsStruct>
    auto set_shader_params_struct() { shader_params_metadata = shader_parameter_metadata_list_of<ParamsStruct>(); }

    std::string source_path;
    std::string source_entry;
    ShaderParameterMetadataList shader_params_metadata;
    std::function<auto(ShaderCompilationEnvironment&) -> void> modify_compiler_environment_func;
};

}
