#pragma once

#include <variant>

#include "resource.hpp"
#include "shader_param.hpp"
#include "shader_compilation_environment.hpp"
#include "../prelude/ref.hpp"

namespace bi::gfx {

enum class SurfaceModel : uint8_t {
    unlit,
    lit,
};

enum class BlendMode : uint8_t {
    opaque,
    alpha_test,
    translucent,
    additive,
    modulate,
};

using MaterialParameter = std::variant<
    float,
    float2,
    float3,
    float4,
    int,
    int2,
    int3,
    int4
>;

struct Material final {
    auto base_material() -> Ref<Material>;
    auto base_material() const -> CRef<Material>;

    auto update_shader_parameter() -> void;
    auto shader_params_metadata() const -> ShaderParameterMetadataList const&;

    auto modify_compiler_environment(ShaderCompilationEnvironment& compilation_environment) -> void;

    auto get_shader_identifier() const -> std::string;

    SurfaceModel surface_model = SurfaceModel::lit;
    BlendMode blend_mode = BlendMode::opaque;
    std::vector<std::pair<std::string, MaterialParameter>> value_params;
    std::vector<std::pair<std::string, Ref<Texture>>> texture_params;
    std::vector<std::pair<std::string, Ref<Sampler>>> sampler_params;
    ShaderParameter shader_parameters;
    std::string material_function;

    Ptr<Material> referenced_material;
};

}
