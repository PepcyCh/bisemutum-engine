#pragma once

#include <bisemutum/runtime/scene_object.hpp>
#include <bisemutum/graphics/resource.hpp>
#include <bisemutum/graphics/shader_param.hpp>
#include <bisemutum/renderer/ddgi_volume.hpp>

namespace bi {

struct DdgiVolumeData final {
    gfx::Texture irradiance;
    gfx::Texture visibility;

    float3 base_point;
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

private:
    auto init_sample_randoms() -> void;

    std::unordered_map<rt::SceneObject::Id, DdgiVolumeData> ddgi_volumes_data_;

    gfx::Buffer sample_randoms_buffer_;
};

}
