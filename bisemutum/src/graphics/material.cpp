#include <bisemutum/graphics/material.hpp>

#include <bisemutum/prelude/math.hpp>
#include <bisemutum/prelude/misc.hpp>
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>

namespace bi::gfx {

namespace {

constexpr std::string_view parameter_prefix = "PARAM_";

} // namespace

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
            list.params[i].var_name = fmt::format("{}{}", parameter_prefix, value_params[i].first);
        }
        for (size_t i = 0; i < texture_params.size(); i++) {
            auto list_i = i + value_params.size();
            list.params[list_i].descriptor_type = rhi::DescriptorType::sampled_texture;
            list.params[list_i].type_name = "Texture2D";
            list.params[list_i].size = sizeof(TextureParam);
            list.params[list_i].alignment = alignof(TextureParam);
            list.params[list_i].cpu_alignment = list.params[list_i].alignment;
            list.params[list_i].var_name = fmt::format("{}{}", parameter_prefix, texture_params[i].first);
        }
        for (size_t i = 0; i < sampler_params.size(); i++) {
            auto list_i = i + value_params.size() + texture_params.size();
            list.params[list_i].descriptor_type = rhi::DescriptorType::sampler;
            list.params[list_i].type_name = "SamplerState";
            list.params[list_i].size = sizeof(shader::SamplerState);
            list.params[list_i].alignment = alignof(shader::SamplerState);
            list.params[list_i].cpu_alignment = list.params[list_i].alignment;
            list.params[list_i].var_name = fmt::format("{}{}", parameter_prefix, sampler_params[i].first);
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

    update_shader_struct();
}
auto Material::shader_params_metadata() const -> ShaderParameterMetadataList const& {
    return shader_parameters.metadata_list();
}

auto Material::update_shader_struct() -> void {
    // update gpu_scene_struct_
    gpu_scene_struct_.clear();

    size_t cpu_size = 0;
    std::vector<std::byte> cpu_data{};
    uint32_t num_padding = 0;
    auto add_field = [&](std::string_view type, std::string_view name) {
        gpu_scene_struct_ += fmt::format("    {} {};\n", type, name);
    };
    auto add_padding = [&](size_t size, size_t alignment) {
        auto new_size = aligned_size(size, alignment);
        for (size_t i = size; i < new_size; i += 4) {
            add_field("float", fmt::format("_padding_{}", num_padding++));
        }
        return new_size;
    };
    auto add_value = [&] <typename T> (T const& value) {
        cpu_data.resize(cpu_size + sizeof(T));
        new (cpu_data.data() + cpu_size) T{value};
        cpu_size += sizeof(T);
    };

    for (auto& [name, value] : value_params) {
        std::visit(
            FunctorsHelper{
                [&](float value) -> void {
                    cpu_size = add_padding(cpu_size, sizeof(float));
                    add_field("float", name);
                    add_value(value);
                },
                [&](float2 const& value) -> void {
                    cpu_size = add_padding(cpu_size, sizeof(float2));
                    add_field("float2", name);
                    add_value(value);
                },
                [&](float3 const& value) -> void {
                    cpu_size = add_padding(cpu_size, sizeof(float4));
                    add_field("float3", name);
                    add_value(value);
                },
                [&](float4 const& value) -> void {
                    cpu_size = add_padding(cpu_size, sizeof(float4));
                    add_field("float4", name);
                    add_value(value);
                },
                [&](int value) -> void {
                    cpu_size = add_padding(cpu_size, sizeof(int));
                    add_field("int", name);
                    add_value(value);
                },
                [&](int2 const& value) -> void {
                    cpu_size = add_padding(cpu_size, sizeof(int2));
                    add_field("int2", name);
                    add_value(value);
                },
                [&](int3 const& value) -> void {
                    cpu_size = add_padding(cpu_size, sizeof(int4));
                    add_field("int3", name);
                    add_value(value);
                },
                [&](int4 const& value) -> void {
                    cpu_size = add_padding(cpu_size, sizeof(int4));
                    add_field("int4", name);
                    add_value(value);
                },
            },
            value
        );
    }
    std::unordered_set<std::string_view> texture_param_names;
    for (auto& [name, tex] : texture_params) {
        uint32_t tex_index = g_engine->graphics_manager()->add_material_texture(tex);
        cpu_size = add_padding(cpu_size, sizeof(uint32_t));
        add_field("uint", name);
        add_value(tex_index);
        texture_param_names.insert(name);
    }
    std::unordered_set<std::string_view> sampler_param_names;
    for (auto& [name, samp] : sampler_params) {
        uint32_t samp_index = g_engine->graphics_manager()->add_material_sampler(samp);
        cpu_size = add_padding(cpu_size, sizeof(uint32_t));
        add_field("uint", name);
        add_value(samp_index);
        sampler_param_names.insert(name);
    }
    cpu_size = add_padding(cpu_size, sizeof(float4));

    // update gpu_scene_struct_buffer_
    gpu_scene_struct_buffer_ =
        g_engine->graphics_manager()->get_material_params_buffers()->allocate(cpu_size, sizeof(float4)).value();
    auto temp_buffer = Buffer(rhi::BufferDesc{
        .size = cpu_size,
        .memory_property = rhi::BufferMemoryProperty::cpu_to_gpu,
    });
    temp_buffer.set_data(cpu_data.data(), cpu_data.size());
    g_engine->graphics_manager()->execute_in_this_frame(
        [this, &temp_buffer](Ref<rhi::CommandEncoder> cmd) {
            rhi::BufferBarrier before_barrier{
                .buffer = gpu_scene_struct_buffer_.allocator()->base_buffer().rhi_buffer(),
                .src_access_type = rhi::ResourceAccessType::storage_resource_read,
                .dst_access_type = rhi::ResourceAccessType::transfer_write,
            };
            cmd->resource_barriers({before_barrier}, {});
            cmd->copy_buffer_to_buffer(
                temp_buffer.rhi_buffer(),
                gpu_scene_struct_buffer_.allocator()->base_buffer().rhi_buffer(),
                rhi::BufferCopyDesc{
                    .src_offset = 0,
                    .dst_offset = gpu_scene_struct_buffer_.offset(),
                    .length = gpu_scene_struct_buffer_.size(),
                }
            );
            rhi::BufferBarrier after_barrier{
                .buffer = gpu_scene_struct_buffer_.allocator()->base_buffer().rhi_buffer(),
                .src_access_type = rhi::ResourceAccessType::transfer_write,
                .dst_access_type = rhi::ResourceAccessType::storage_resource_read,
            };
            cmd->resource_barriers({after_barrier}, {});
        }
    );

    // update gpu_scene_material_function_
    std::string temp_gpu_scene_material_function{};
    size_t p_mat_func = 0;
    for (
        size_t pos = 0;
        (pos = material_function.find(parameter_prefix.data(), pos, parameter_prefix.size())) != std::string::npos;
    ) {
        temp_gpu_scene_material_function += material_function.substr(p_mat_func, pos - p_mat_func);

        auto name_l = pos + parameter_prefix.size();
        auto name_r = name_l;
        while (name_r < material_function.size() && is_identifier_ch(material_function[name_r])) { ++name_r; }
        auto name = std::string_view{material_function.data() + name_l, name_r - name_l};

        if (texture_param_names.contains(name)) {
            temp_gpu_scene_material_function += fmt::format("material_textures[NonUniformResourceIndex(PARAM.{})]", name);
        } else if (sampler_param_names.contains(name)) {
            temp_gpu_scene_material_function += fmt::format("material_samplers[NonUniformResourceIndex(PARAM.{})]", name);
        } else {
            temp_gpu_scene_material_function += fmt::format("PARAM.{}", name);
        }

        pos += parameter_prefix.size() + name.size();
        p_mat_func = pos;
    }
    temp_gpu_scene_material_function += material_function.substr(p_mat_func);

    gpu_scene_material_function_.clear();
    p_mat_func = 0;
    for (
        size_t pos = 0;
        (pos = temp_gpu_scene_material_function.find(".Sample(", pos, 8)) != std::string::npos;
    ) {
        gpu_scene_material_function_ += temp_gpu_scene_material_function.substr(p_mat_func, pos - p_mat_func);

        auto pos_r = pos + 8;
        size_t num_paren = 0;
        while (pos_r < temp_gpu_scene_material_function.size()) {
            if (temp_gpu_scene_material_function[pos_r] == '(') {
                ++num_paren;
            } else if (temp_gpu_scene_material_function[pos_r] == ')') {
                if (num_paren == 0) { break; }
                --num_paren;
            }
            ++pos_r;
        }
        gpu_scene_material_function_ += ".SampleLevel(";
        gpu_scene_material_function_ += temp_gpu_scene_material_function.substr(pos + 8, pos_r - (pos + 8));
        gpu_scene_material_function_ += ", 0)";

        pos = pos_r + 1;
        p_mat_func = pos;
    }
    gpu_scene_material_function_ += temp_gpu_scene_material_function.substr(p_mat_func);
}

auto Material::modify_compiler_environment(ShaderCompilationEnvironment& compilation_environment) -> void {
    modify_compiler_environment_common(compilation_environment);

    compilation_environment.set_replace_arg("MATERIAL_FUNCTION", material_function);
}

auto Material::modify_compiler_environment_for_gpu_scene(ShaderCompilationEnvironment& compilation_environment) -> void {
    modify_compiler_environment_common(compilation_environment);

    compilation_environment.set_replace_arg("MATERIAL_FUNCTION", gpu_scene_material_function_);

    compilation_environment.set_replace_arg("RAYTRACING_MATERIAL_STRUCT", gpu_scene_struct_);
}

auto Material::modify_compiler_environment_common(ShaderCompilationEnvironment& compilation_environment) -> void {
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
}

auto Material::get_shader_identifier() const -> std::string {
    return fmt::format("{:x}", std::hash<std::string>{}(base_material()->material_function));
}

}
