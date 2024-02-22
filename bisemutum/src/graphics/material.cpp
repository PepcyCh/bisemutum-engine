#include <bisemutum/graphics/material.hpp>

#include <bisemutum/prelude/math.hpp>
#include <bisemutum/prelude/misc.hpp>

namespace bi::gfx {

auto Material::base_material() -> Ref<Material> {
    return referenced_material ? referenced_material.value() : unsafe_make_ref(this);
}
auto Material::base_material() const -> CRef<Material> {
    return referenced_material ? referenced_material.value() : unsafe_make_cref(this);
}

auto Material::update_shader_parameter() -> void {
    // TODO - material dirty
    if (!shader_parameters) {
        ShaderParameterMetadataList list{};
        list.params.resize(value_params.size() + texture_params.size() + sampler_params.size());
        for (size_t i = 0; i < value_params.size(); i++) {
            std::visit(
                FunctorsHelper{
                    [i, &list](float) -> void {
                        list.params[i].descriptor_type = rhi::DescriptorType::none;
                        list.params[i].type_name = "float";
                        list.params[i].size = sizeof(float);
                        list.params[i].alignment = sizeof(float);
                    },
                    [i, &list](float2) -> void {
                        list.params[i].descriptor_type = rhi::DescriptorType::none;
                        list.params[i].type_name = "float2";
                        list.params[i].size = sizeof(float2);
                        list.params[i].alignment = sizeof(float2);
                    },
                    [i, &list](float3) -> void {
                        list.params[i].descriptor_type = rhi::DescriptorType::none;
                        list.params[i].type_name = "float3";
                        list.params[i].size = sizeof(float3);
                        list.params[i].alignment = sizeof(float4);
                    },
                    [i, &list](float4) -> void {
                        list.params[i].descriptor_type = rhi::DescriptorType::none;
                        list.params[i].type_name = "float4";
                        list.params[i].size = sizeof(float4);
                        list.params[i].alignment = sizeof(float4);
                    },
                    [i, &list](int) -> void {
                        list.params[i].descriptor_type = rhi::DescriptorType::none;
                        list.params[i].type_name = "int";
                        list.params[i].size = sizeof(int);
                        list.params[i].alignment = sizeof(int);
                    },
                    [i, &list](int2) -> void {
                        list.params[i].descriptor_type = rhi::DescriptorType::none;
                        list.params[i].type_name = "int2";
                        list.params[i].size = sizeof(int2);
                        list.params[i].alignment = sizeof(int2);
                    },
                    [i, &list](int3) -> void {
                        list.params[i].descriptor_type = rhi::DescriptorType::none;
                        list.params[i].type_name = "int3";
                        list.params[i].size = sizeof(int3);
                        list.params[i].alignment = sizeof(int4);
                    },
                    [i, &list](int4) -> void {
                        list.params[i].descriptor_type = rhi::DescriptorType::none;
                        list.params[i].type_name = "int4";
                        list.params[i].size = sizeof(int4);
                        list.params[i].alignment = sizeof(int4);
                    },
                },
                value_params[i].second
            );
            list.params[i].cpu_alignment = list.params[i].alignment;
            list.params[i].var_name = value_params[i].first;
        }
        for (size_t i = 0; i < texture_params.size(); i++) {
            auto list_i = i + value_params.size();
            list.params[list_i].descriptor_type = rhi::DescriptorType::sampled_texture;
            list.params[list_i].type_name = "Texture2D";
            list.params[list_i].size = sizeof(TextureParam);
            list.params[list_i].alignment = alignof(TextureParam);
            list.params[list_i].cpu_alignment = list.params[list_i].alignment;
            list.params[list_i].var_name = texture_params[i].first;
        }
        for (size_t i = 0; i < sampler_params.size(); i++) {
            auto list_i = i + value_params.size() + texture_params.size();
            list.params[list_i].descriptor_type = rhi::DescriptorType::sampler;
            list.params[list_i].type_name = "SamplerState";
            list.params[list_i].size = sizeof(shader::SamplerState);
            list.params[list_i].alignment = alignof(shader::SamplerState);
            list.params[list_i].cpu_alignment = list.params[list_i].alignment;
            list.params[list_i].var_name = sampler_params[i].first;
        }
        shader_parameters.initialize(std::move(list), true);
    }

    size_t cpu_size = 0;
    for (auto& [_, value] : value_params) {
        std::visit(
            FunctorsHelper{
                [this, &cpu_size](float value) -> void {
                    cpu_size = aligned_size(cpu_size, sizeof(float));
                    *shader_parameters.mutable_typed_data_offset<float>(cpu_size) = value;
                    cpu_size += sizeof(float);
                },
                [this, &cpu_size](float2 const& value) -> void {
                    cpu_size = aligned_size(cpu_size, sizeof(float2));
                    *shader_parameters.mutable_typed_data_offset<float2>(cpu_size) = value;
                    cpu_size += sizeof(float2);
                },
                [this, &cpu_size](float3 const& value) -> void {
                    cpu_size = aligned_size(cpu_size, sizeof(float4));
                    *shader_parameters.mutable_typed_data_offset<float3>(cpu_size) = value;
                    cpu_size += sizeof(float3);
                },
                [this, &cpu_size](float4 const& value) -> void {
                    cpu_size = aligned_size(cpu_size, sizeof(float4));
                    *shader_parameters.mutable_typed_data_offset<float4>(cpu_size) = value;
                    cpu_size += sizeof(float4);
                },
                [this, &cpu_size](int value) -> void {
                    cpu_size = aligned_size(cpu_size, sizeof(int));
                    *shader_parameters.mutable_typed_data_offset<int>(cpu_size) = value;
                    cpu_size += sizeof(int);
                },
                [this, &cpu_size](int2 const& value) -> void {
                    cpu_size = aligned_size(cpu_size, sizeof(int2));
                    *shader_parameters.mutable_typed_data_offset<int2>(cpu_size) = value;
                    cpu_size += sizeof(int2);
                },
                [this, &cpu_size](int3 const& value) -> void {
                    cpu_size = aligned_size(cpu_size, sizeof(int4));
                    *shader_parameters.mutable_typed_data_offset<int3>(cpu_size) = value;
                    cpu_size += sizeof(int3);
                },
                [this, &cpu_size](int4 const& value) -> void {
                    cpu_size = aligned_size(cpu_size, sizeof(int4));
                    *shader_parameters.mutable_typed_data_offset<int4>(cpu_size) = value;
                    cpu_size += sizeof(int4);
                },
            },
            value
        );
    }
    for (auto& [_, tex] : texture_params) {
        cpu_size = aligned_size(cpu_size, alignof(TextureParam));
        auto texture = shader_parameters.mutable_typed_data_offset<TextureParam>(cpu_size);
        texture->texture = tex;
        texture->base_layer = 0;
        texture->num_layers = ~0u;
        texture->base_level = 0;
        texture->num_levels = ~0u;
        cpu_size += sizeof(TextureParam);
    }
    for (auto& [_, samp] : sampler_params) {
        cpu_size = aligned_size(cpu_size, alignof(shader::SamplerState));
        auto sampler = shader_parameters.mutable_typed_data_offset<shader::SamplerState>(cpu_size);
        sampler->sampler = samp;
        cpu_size += sizeof(shader::SamplerState);
    }
}
auto Material::shader_params_metadata() const -> ShaderParameterMetadataList const& {
    return shader_parameters.metadata_list();
}

auto Material::modify_compiler_environment(ShaderCompilationEnvironment& compilation_environment) -> void {
    compilation_environment.set_define("MATERIAL_SURFACE_MODEL", static_cast<uint32_t>(surface_model));

    switch (blend_mode) {
        case BlendMode::opaque:
            compilation_environment.set_define("MATERIAL_BLEND_MODE_OPAQUE");
            break;
        case BlendMode::alpha_test:
            compilation_environment.set_define("MATERIAL_BLEND_MODE_ALPHA_TEST");
            break;
        case BlendMode::translucent:
            compilation_environment.set_define("MATERIAL_BLEND_MODE_TRANSLUCENT");
            break;
        case BlendMode::additive:
            compilation_environment.set_define("MATERIAL_BLEND_MODE_ADDITIVE");
            break;
        case BlendMode::modulate:
            compilation_environment.set_define("MATERIAL_BLEND_MODE_MODULATE");
            break;
    }

    compilation_environment.set_replace_arg("MATERIAL_FUNCTION", material_function);
}

auto Material::get_shader_identifier() const -> std::string {
    return fmt::format("{:x}", std::hash<std::string>{}(base_material()->material_function));
}

}
