#pragma once

#include <bisemutum/graphics/shader_param.hpp>

namespace bi::gfx {

inline constexpr size_t max_num_material_textures = 1024;

BI_SHADER_PARAMETERS_BEGIN(GpuSceneData)
    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<float>, positions_buffer)
    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<float>, normals_buffer)
    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<float>, tangents_buffer)
    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<float>, colors_buffer)
    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<float>, texcoords_buffer)
    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<float>, texcoords2_buffer)
    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<uint>, indices_buffer)
    BI_SHADER_PARAMETER_SRV_BUFFER(StructuredBuffer<float4x4>, history_transforms_buffer)

    BI_SHADER_PARAMETER_SRV_BUFFER(ByteAddressBuffer, material_params)

    BI_SHADER_PARAMETER_SRV_TEXTURE_ARRAY(Texture2D, material_textures, [max_num_material_textures])
    BI_SHADER_PARAMETER_SAMPLER_ARRAY(SamplerState, material_samplers, [max_num_material_textures])
BI_SHADER_PARAMETERS_END()

}
