#pragma once

#include <bisemutum/runtime/scene_object.hpp>
#include <bisemutum/graphics/resource.hpp>
#include <bisemutum/graphics/shader_param.hpp>
#include <bisemutum/graphics/handles.hpp>
#include <bisemutum/renderer/basic.hpp>
#include <bisemutum/renderer/ddgi_volume.hpp>

namespace bi {

struct DdgiVolumeInfo final {
    uint32_t index = 0xffffffffu;
    uint32_t prev_index = 0xffffffffu;

    float3 base_position;
    float3 frame_x;
    float3 frame_y;
    float3 frame_z;
    float3 voluem_extent;
};

BI_SHADER_PARAMETERS_BEGIN(DdgiVolumeData)
    BI_SHADER_PARAMETER(float3, base_position);
    BI_SHADER_PARAMETER(float, _pad);
    BI_SHADER_PARAMETER(float3, frame_x);
    BI_SHADER_PARAMETER(float, extent_x);
    BI_SHADER_PARAMETER(float3, frame_y);
    BI_SHADER_PARAMETER(float, extent_y);
    BI_SHADER_PARAMETER(float3, frame_z);
    BI_SHADER_PARAMETER(float, extent_z);
BI_SHADER_PARAMETERS_END()

BI_SHADER_PARAMETERS_BEGIN(DdgiVolumeLightingData)
    BI_SHADER_PARAMETER(uint, num_volumes);
    BI_SHADER_PARAMETER(float, strength);
    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<DdgiVolumeData>, volumes);
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2DArray, irradiance_texture);
    BI_SHADER_PARAMETER_SRV_TEXTURE(Texture2DArray, visibility_texture);
    BI_SHADER_PARAMETER_SAMPLER(SamplerState, sampler);
BI_SHADER_PARAMETERS_END()

struct DdgiContext final {
    DdgiContext();

    auto update_frame(BasicRenderer::IndirectDiffuseSettings const& settings) -> void;

    auto num_ddgi_volumes() const -> uint32_t;

    auto for_each_ddgi_volume(std::function<auto(DdgiVolumeInfo const&) -> void> func) const -> void;

    auto sample_randoms_buffer() -> Ref<gfx::Buffer> { return sample_randoms_buffer_; }

    auto get_shader_data() const -> DdgiVolumeLightingData const& { return shader_data_[db_ind_]; }
    auto get_shader_data_prev() const -> DdgiVolumeLightingData const& { return shader_data_[db_ind_ ^ 1]; }

    auto is_texture_valid() const -> bool { return irradiance_texture_[db_ind_] && visibility_texture_[db_ind_]; }
    auto get_irradiance_texture() -> Ref<gfx::Texture> { return irradiance_texture_[db_ind_].ref(); }
    auto get_visibility_texture() -> Ref<gfx::Texture> { return visibility_texture_[db_ind_].ref(); }
    auto is_texture_prev_valid() const -> bool { return irradiance_texture_[db_ind_ ^ 1] && visibility_texture_[db_ind_ ^ 1]; }
    auto get_irradiance_texture_prev() -> Ref<gfx::Texture> { return irradiance_texture_[db_ind_ ^ 1].ref(); }
    auto get_visibility_texture_prev() -> Ref<gfx::Texture> { return visibility_texture_[db_ind_ ^ 1].ref(); }
    auto set_irradiance_texture(gfx::TextureHandle irradiance) -> void;
    auto set_visibility_texture(gfx::TextureHandle visibility) -> void;

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

    std::unordered_map<rt::SceneObject::Id, DdgiVolumeInfo> ddgi_volumes_data_;

    gfx::Buffer sample_randoms_buffer_;

    std::array<gfx::Buffer, 2> volumes_buffer_;
    std::array<DdgiVolumeLightingData, 2> shader_data_;

    Ptr<gfx::Sampler> sampler_;
    std::array<Box<gfx::Texture>, 2> irradiance_texture_;
    std::array<Box<gfx::Texture>, 2> visibility_texture_;

    uint32_t db_ind_ = 0;
    uint64_t last_frame_ = 0;
};

}
