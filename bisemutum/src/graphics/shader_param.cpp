#include <bisemutum/graphics/shader_param.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/window/window.hpp>
#include <bisemutum/rhi/device.hpp>
#include <bisemutum/prelude/math.hpp>
#include <bisemutum/prelude/misc.hpp>

#include "descriptor_sets.hpp"

namespace bi::gfx {

namespace {

auto arrays_size_to_string(std::vector<uint32_t> const& array_sizes) -> std::string {
    std::string str = "";
    for (auto sz : array_sizes) {
        str += fmt::format("[{}]", sz);
    }
    return str;
}

auto format_to_string(rhi::ResourceFormat format) -> char const* {
    switch (format) {
        case rhi::ResourceFormat::r8_unorm: return "r8";
        case rhi::ResourceFormat::r8_snorm: return "r8snorm";
        case rhi::ResourceFormat::r8_uint: return "r8ui";
        case rhi::ResourceFormat::r8_sint: return "r8i";
        case rhi::ResourceFormat::rg8_unorm: return "rg8";
        case rhi::ResourceFormat::rg8_snorm: return "rg8snorm";
        case rhi::ResourceFormat::rg8_uint: return "rg8ui";
        case rhi::ResourceFormat::rg8_sint: return "rg8i";
        case rhi::ResourceFormat::rgba8_unorm: return "rgba8";
        case rhi::ResourceFormat::rgba8_snorm: return "rgba8snorm";
        case rhi::ResourceFormat::rgba8_uint: return "rgba8ui";
        case rhi::ResourceFormat::rgba8_sint: return "rgba8i";
        case rhi::ResourceFormat::a2rgb10_unorm: return "rgb10a2";
        case rhi::ResourceFormat::a2rgb10_uint: return "rgb10a2ui";
        case rhi::ResourceFormat::r16_unorm: return "r16";
        case rhi::ResourceFormat::r16_snorm: return "r16snorm";
        case rhi::ResourceFormat::r16_uint: return "r16ui";
        case rhi::ResourceFormat::r16_sint: return "r16i";
        case rhi::ResourceFormat::r16_sfloat: return "r16f";
        case rhi::ResourceFormat::rg16_unorm: return "rg16";
        case rhi::ResourceFormat::rg16_snorm: return "rg16snorm";
        case rhi::ResourceFormat::rg16_uint: return "rg16ui";
        case rhi::ResourceFormat::rg16_sint: return "rg16i";
        case rhi::ResourceFormat::rg16_sfloat: return "rg16f";
        case rhi::ResourceFormat::rgba16_unorm: return "rgba16";
        case rhi::ResourceFormat::rgba16_snorm: return "rgba16snorm";
        case rhi::ResourceFormat::rgba16_uint: return "rgba16ui";
        case rhi::ResourceFormat::rgba16_sint: return "rgba16i";
        case rhi::ResourceFormat::rgba16_sfloat: return "rgba16f";
        case rhi::ResourceFormat::r32_uint: return "r32ui";
        case rhi::ResourceFormat::r32_sint: return "r32i";
        case rhi::ResourceFormat::r32_sfloat: return "r32f";
        case rhi::ResourceFormat::rg32_uint: return "rg32ui";
        case rhi::ResourceFormat::rg32_sint: return "rg32i";
        case rhi::ResourceFormat::rg32_sfloat: return "rg32f";
        case rhi::ResourceFormat::rgba32_uint: return "rgba32ui";
        case rhi::ResourceFormat::rgba32_sint: return "rgba32i";
        case rhi::ResourceFormat::rgba32_sfloat: return "rgba32f";
        case rhi::ResourceFormat::b10gr11_ufloat: return "r11g11b10f";
        default: return "unknown";
    }
}

}

auto ShaderParameterMetadataList::bind_group_layout(
    uint32_t set, BitFlags<rhi::ShaderStage> visibility
) const -> rhi::BindGroupLayout {
    rhi::BindGroupLayout layout{};

    auto uniform_buffer_empty = true;
    auto& uniform_buffer = layout.emplace_back();
    uniform_buffer.type = rhi::DescriptorType::uniform_buffer;
    uniform_buffer.count = 1;
    uniform_buffer.space = set;
    uniform_buffer.binding_or_register = 0;
    uniform_buffer.visibility = visibility;
    auto curr_binding = 1;

    for (auto& param : params) {
        uint32_t count = 1;
        for (auto sz : param.array_sizes) { count *= sz; }

        if (param.descriptor_type == rhi::DescriptorType::none) {
            uniform_buffer_empty = false;
        } else {
            auto& entry = layout.emplace_back();
            entry.type = param.descriptor_type;
            entry.count = count;
            entry.space = set;
            entry.binding_or_register = curr_binding;
            entry.visibility = visibility;
            curr_binding += count;
        }
    }

    if (uniform_buffer_empty) {
        layout.erase(layout.begin());
    }

    return layout;
}

auto ShaderParameterMetadataList::generated_shader_definition(
    uint32_t set, uint32_t samplers_set
) const -> std::string {
    std::string uniform_buffer{};
    std::string other_bindings{};

    uint32_t curr_binding = 1;
    auto separate_samplers = g_engine->graphics_manager()->device()->properties().separate_sampler_heap;
    for (auto& param : params) {
        auto array = arrays_size_to_string(param.array_sizes);
        uint32_t count = 1;
        for (auto sz : param.array_sizes) { count *= sz; }

        switch (param.descriptor_type) {
            case rhi::DescriptorType::none:
                uniform_buffer += fmt::format("{} {}{};\n", param.type_name, param.var_name, array);
                break;
            case rhi::DescriptorType::sampler: {
                auto sampler_binding = curr_binding;
                auto sampler_set = set;
                if (separate_samplers) {
                    sampler_binding += samplers_binding_shift * set;
                    sampler_set = samplers_set;
                }
                other_bindings += fmt::format(
                    "[[vk::binding({}, {})]] SamplerState {}{} : register(s{}, space{});\n",
                    sampler_binding, sampler_set, param.var_name, array, sampler_binding, sampler_set
                );
                curr_binding += count;
                break;
            }
            case rhi::DescriptorType::uniform_buffer:
                other_bindings += fmt::format(
                    "[[vk::binding({}, {})]] {} {}{} : register(b{}, space{});\n",
                    curr_binding, set, param.type_name, param.var_name, array, curr_binding, set
                );
                curr_binding += count;
                break;
            case rhi::DescriptorType::read_only_storage_buffer:
            case rhi::DescriptorType::sampled_texture:
            case rhi::DescriptorType::read_only_storage_texture:
            case rhi::DescriptorType::acceleration_structure:
                other_bindings += fmt::format(
                    "[[vk::binding({}, {})]] {} {}{} : register(t{}, space{});\n",
                    curr_binding, set, param.type_name, param.var_name, array, curr_binding, set
                );
                curr_binding += count;
                break;
            case rhi::DescriptorType::read_write_storage_buffer:
                other_bindings += fmt::format(
                    "[[vk::binding({}, {})]] {} {}{} : register(u{}, space{});\n",
                    curr_binding, set, param.type_name, param.var_name, array, curr_binding, set
                );
                curr_binding += count;
                break;
            case rhi::DescriptorType::read_write_storage_texture:
                other_bindings += fmt::format(
                    "[[vk::binding({}, {}), vk::image_format(\"{}\")]] {} {}{} : register(u{}, space{});\n",
                    curr_binding, set, format_to_string(param.format), param.type_name, param.var_name, array, curr_binding, set
                );
                curr_binding += count;
                break;
            default: break;
        }
    }

    if (!uniform_buffer.empty()) {
        return fmt::format(
            "[[vk::binding(0, {})]] cbuffer _cbuffer_{} : register(b0, space{}) {{\n{}}};\n{}",
            set, set, set, uniform_buffer, other_bindings
        );
    } else {
        return other_bindings;
    }
}


auto ShaderParameter::initialize(ShaderParameterMetadataList metadata_list, bool use_gpu_only_memory) -> void {
    if (allocated_) { return; }

    metadata_list_ = std::move(metadata_list);

    size_t size = 0;
    size_t alignment = 0;
    size_t cpu_size = 0;
    size_t cpu_alignment = 0;
    std::vector<UniformRange> temp_uniform_ranges{};

    for (auto& param : metadata_list_.params) {
        uint32_t count = 1;
        for (auto sz : param.array_sizes) { count *= sz; }

        if (param.descriptor_type == rhi::DescriptorType::none) {
            for (uint32_t i = 0; i < count; i++) {
                size = aligned_size(size, param.alignment);
                cpu_size = aligned_size(cpu_size, param.cpu_alignment);

                temp_uniform_ranges.emplace_back(cpu_size, size, param.size);

                size += param.size;
                cpu_size += param.size;
            }
            alignment = std::max(alignment, param.alignment);
        } else {
            for (uint32_t i = 0; i < count; i++) {
                cpu_size = aligned_size(cpu_size, param.cpu_alignment) + param.size;
            }
        }
        cpu_alignment = std::max(cpu_alignment, param.cpu_alignment);
    }
    size = aligned_size(size, alignment);
    cpu_size = aligned_size(cpu_size, cpu_alignment);

    data_.resize(cpu_size == 0 ? 0 : cpu_size + cpu_alignment - 1);
    auto start_address = aligned_size(reinterpret_cast<size_t>(data_.data()), cpu_alignment);
    data_start_ = reinterpret_cast<std::byte*>(start_address);

    if (size > 0) {
        uniform_buffer_ = Buffer(rhi::BufferDesc{
            .size = size,
            .usages = rhi::BufferUsage::uniform,
            .memory_property = use_gpu_only_memory
                ? rhi::BufferMemoryProperty::gpu_only : rhi::BufferMemoryProperty::cpu_to_gpu,
            .persistently_mapped = false,
        });
    }

    for (size_t i = 0; i < temp_uniform_ranges.size(); i++) {
        size_t j = i + 1;
        while (j < temp_uniform_ranges.size()) {
            if (
                temp_uniform_ranges[j].cpu_offset - temp_uniform_ranges[i].cpu_offset
                != temp_uniform_ranges[j].gpu_offset - temp_uniform_ranges[i].gpu_offset
            ) {
                break;
            }
            ++j;
        }
        auto range_size = temp_uniform_ranges[j - 1].cpu_offset + temp_uniform_ranges[j - 1].size
            - temp_uniform_ranges[i].cpu_offset;
        uniform_ranges_.emplace_back(temp_uniform_ranges[i].cpu_offset, temp_uniform_ranges[i].gpu_offset, range_size);
        i = j;
    }

    allocated_ = true;
}

auto ShaderParameter::reset() -> void {
    if (allocated_) {
        data_.clear();
        uniform_buffer_.reset();
        allocated_ = false;
    }
}

auto ShaderParameter::update_uniform_buffer() -> void {
    auto need_to_update = g_engine->window()->frame_count() != last_update_frame_;
    last_update_frame_ = g_engine->window()->frame_count();
    if (need_to_update && dirty_count_ > 0) {
        std::vector<Buffer::DataSetDesc> data_set_descs(uniform_ranges_.size());
        for (size_t i = 0; i < uniform_ranges_.size(); i++) {
            data_set_descs[i].data = data_start_ + uniform_ranges_[i].cpu_offset;
            data_set_descs[i].size = uniform_ranges_[i].size;
            data_set_descs[i].offset = uniform_ranges_[i].gpu_offset;
        }
        uniform_buffer_.set_multiple_data_raw(data_set_descs);
        --dirty_count_;
    }
}

auto ShaderParameter::data() const -> std::byte const* {
    return data_start_;
}
auto ShaderParameter::mutable_data() -> std::byte* {
    dirty_count_ = g_engine->graphics_manager()->num_frames_in_flight();
    last_update_frame_ = 0;
    return data_start_;
}

auto ShaderParameter::data_offset(size_t offset) const -> std::byte const* {
    return data_start_ + offset;
}
auto ShaderParameter::mutable_data_offset(size_t offset) -> std::byte* {
    dirty_count_ = g_engine->graphics_manager()->num_frames_in_flight();
    last_update_frame_ = 0;
    return data_start_ + offset;
}

}
