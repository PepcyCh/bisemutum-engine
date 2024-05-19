#pragma once

#include <bisemutum/runtime/scene_object.hpp>
#include <bisemutum/graphics/resource.hpp>
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

struct DdgiContext final {
    auto update_frame() -> void;

private:
    std::unordered_map<rt::SceneObject::Id, DdgiVolumeData> ddgi_volumes_data_;

    gfx::Buffer sample_randoms_buffer_;
};

}
