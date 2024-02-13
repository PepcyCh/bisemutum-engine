#pragma once

#include "vertex_attributes_defines.hlsl"

#define VERTEX_ATTRIBUTES_TYPE_NONE 0
#define VERTEX_ATTRIBUTES_TYPE_POSITION_ONLY 1
#define VERTEX_ATTRIBUTES_TYPE_POSITION_TEXCOORD 2
#define VERTEX_ATTRIBUTES_TYPE_FULL 3

#ifndef VERTEX_ATTRIBUTES_TYPE
#define VERTEX_ATTRIBUTES_TYPE VERTEX_ATTRIBUTES_TYPE_NONE
#endif

struct VertexAttributes {
    INPUT_VERTEX_POSITION(float3 position);
#if VERTEX_ATTRIBUTES_TYPE >= VERTEX_ATTRIBUTES_TYPE_FULL
    INPUT_VERTEX_NORMAL(float3 normal);
    INPUT_VERTEX_TANGENT(float4 tangent);
#endif
#if VERTEX_ATTRIBUTES_TYPE >= VERTEX_ATTRIBUTES_TYPE_POSITION_TEXCOORD
    INPUT_VERTEX_TEXCOORD0(float2 texcoord0);
#endif
};

struct VertexAttributesOutput {
    float4 sv_position : SV_Position;
#if VERTEX_ATTRIBUTES_TYPE >= VERTEX_ATTRIBUTES_TYPE_POSITION_ONLY
    float3 position_world : TEXCOORD0;
#endif
#if VERTEX_ATTRIBUTES_TYPE >= VERTEX_ATTRIBUTES_TYPE_FULL
    float3 normal_world : TEXCOORD1;
    float3 tangent_world : TEXCOORD2;
    float3 bitangent_world : TEXCOORD3;
#endif
#if VERTEX_ATTRIBUTES_TYPE >= VERTEX_ATTRIBUTES_TYPE_POSITION_TEXCOORD
    float2 texcoord0 : TEXCOORD4;
#endif
};
