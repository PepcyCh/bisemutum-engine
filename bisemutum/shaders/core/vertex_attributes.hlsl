#pragma once

#include "vertex_attributes_defines.hlsl"

struct VertexAttributes {
    INPUT_VERTEX_POSITION(float3 position);
#if !VERTEX_ATTRIBUTES_TYPE_POSITION_ONLY
    INPUT_VERTEX_NORMAL(float3 normal);
    INPUT_VERTEX_TANGENT(float4 tangent);
    INPUT_VERTEX_TEXCOORD0(float2 texcoord0);
#endif
};

struct VertexAttributesOutput {
    float4 sv_position : SV_POSITION;
#if !VERTEX_ATTRIBUTES_TYPE_POSITION_ONLY
    float3 position_world : TEXCOORD0;
    float3 normal_world : TEXCOORD1;
    float3 tangent_world : TEXCOORD2;
    float3 bitangent_world : TEXCOORD3;
    float2 texcoord0 : TEXCOORD4;
#endif
};
