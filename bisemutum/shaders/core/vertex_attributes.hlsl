#pragma once

#include "vertex_attributes_defines.hlsl"

#define VA_TYPE_NONE 0
#define VA_TYPE_POSITION 0x1
#define VA_TYPE_HISTORY_POSITION 0x2
#define VA_TYPE_NORMAL 0x4
#define VA_TYPE_TANGENT 0x8
#define VA_TYPE_COLOR 0x10
#define VA_TYPE_TEXCOORD 0x20
#define VA_TYPE_TEXCOORD2 0x40

#ifndef VERTEX_ATTRIBUTES_IN
#define VERTEX_ATTRIBUTES_IN VA_TYPE_NONE
#endif

#ifndef VERTEX_ATTRIBUTES_OUT
#define VERTEX_ATTRIBUTES_OUT VA_TYPE_NONE
#endif

struct VertexAttributes {
#if (VERTEX_ATTRIBUTES_IN & VA_TYPE_POSITION) != 0
    INPUT_VERTEX_POSITION(float3 position);
#endif
#if (VERTEX_ATTRIBUTES_IN & VA_TYPE_NORMAL) != 0
    INPUT_VERTEX_NORMAL(float3 normal);
#endif
#if (VERTEX_ATTRIBUTES_IN & VA_TYPE_TANGENT) != 0
    INPUT_VERTEX_TANGENT(float4 tangent);
#endif
#if (VERTEX_ATTRIBUTES_IN & VA_TYPE_COLOR) != 0
    INPUT_VERTEX_COLOR(float3 color);
#endif
#if (VERTEX_ATTRIBUTES_IN & VA_TYPE_TEXCOORD) != 0
    INPUT_VERTEX_TEXCOORD0(float2 texcoord);
#endif
#if (VERTEX_ATTRIBUTES_IN & VA_TYPE_TEXCOORD2) != 0
    INPUT_VERTEX_TEXCOORD1(float2 texcoord2);
#endif
};

struct VertexAttributesOutput {
    float4 sv_position : SV_Position;
#if (VERTEX_ATTRIBUTES_OUT & VA_TYPE_POSITION) != 0
    float3 position_world : TEXCOORD0;
#endif
#if (VERTEX_ATTRIBUTES_OUT & VA_TYPE_HISTORY_POSITION) != 0
    float3 history_position_world : TEXCOORD1;
#endif
#if (VERTEX_ATTRIBUTES_OUT & VA_TYPE_NORMAL) != 0
    float3 normal_world : TEXCOORD2;
#endif
#if (VERTEX_ATTRIBUTES_OUT & VA_TYPE_TANGENT) != 0
    float3 tangent_world : TEXCOORD3;
    float3 bitangent_world : TEXCOORD4;
#endif
#if (VERTEX_ATTRIBUTES_OUT & VA_TYPE_COLOR) != 0
    float3 color : TEXCOORD5;
#endif
#if (VERTEX_ATTRIBUTES_OUT & VA_TYPE_TEXCOORD) != 0
    float2 texcoord : TEXCOORD6;
#endif
#if (VERTEX_ATTRIBUTES_OUT & VA_TYPE_TEXCOORD2) != 0
    float2 texcoord2 : TEXCOORD7;
#endif
};
