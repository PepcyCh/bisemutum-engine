#pragma once

#include <bisemutum/runtime/scene_object.hpp>
#include <bisemutum/graphics/resource.hpp>
#include <bisemutum/graphics/shader_param.hpp>
#include <bisemutum/renderer/ddgi_volume.hpp>

namespace bi {

struct DdgiVolumeData final {
    uint32_t index = 0xffffffffu;
    uint32_t prev_index = 0xffffffffu;

    float3 base_position;
    float3 frame_x;
    float3 frame_y;
    float3 frame_z;
    float3 voluem_extent;
};

BI_SHADER_PARAMETERS_BEGIN(DdgiVolumeShaderData)
    BI_SHADER_PARAMETER(float3, base_position);
    BI_SHADER_PARAMETER(float, _pad);
    BI_SHADER_PARAMETER(float3, frame_x);
    BI_SHADER_PARAMETER(float, extent_x);
    BI_SHADER_PARAMETER(float3, frame_y);
    BI_SHADER_PARAMETER(float, extent_y);
    BI_SHADER_PARAMETER(float3, frame_z);
    BI_SHADER_PARAMETER(float, extent_z);
BI_SHADER_PARAMETERS_END()

struct DdgiContext final {
    auto update_frame() -> void;

    auto num_ddgi_volumes() const -> uint32_t;

    auto for_each_ddgi_volume(std::function<auto(DdgiVolumeData const&) -> void> func) const -> void;

    auto sample_randoms_buffer() -> Ref<gfx::Buffer> { return sample_randoms_buffer_; }

    static constexpr uint32_t probes_size = 8;
    static constexpr uint32_t probes_total_count = probes_size * probes_size * probes_size;
    static constexpr uint32_t num_rays_per_probe = 64;

    static constexpr uint32_t probe_irradiance_size = 8;
    static constexpr uint32_t irradiance_texture_width = probes_size * probes_size * probe_irradiance_size;
    static constexpr uint32_t irradiance_texture_height = probes_size * probe_irradiance_size;
    static constexpr rhi::ResourceFormat irradiance_texture_format = rhi::ResourceFormat::rgba16_sfloat;

    static constexpr uint32_t probe_visibility_size = 16;
    static constexpr uint32_t visibility_texture_width = probes_size * probes_size * probe_visibility_size;
    static constexpr uint32_t visibility_texture_height = probes_size * probe_visibility_size;
    static constexpr rhi::ResourceFormat visibility_texture_format = rhi::ResourceFormat::rg16_sfloat;

private:
    auto init_sample_randoms() -> void;

    std::unordered_map<rt::SceneObject::Id, DdgiVolumeData> ddgi_volumes_data_;

    gfx::Buffer sample_randoms_buffer_;
};

}
