#pragma once

#include "sbt_record_utils.hlsl"
#include "../material.hlsl"
#include "../vertex_attributes.hlsl"

struct DrawableSbtData {
    uint drawable_index;
    uint position_offset;
    uint normal_offset;
    uint tangent_offset;
    uint color_offset;
    uint texcoord_offset;
    uint texcoord2_offset;
    uint index_offset;
    uint material_offset;
};
[[vk::shader_record_ext]]
ConstantBuffer<DrawableSbtData> drawable_sbt_record : register(SBT_RECORD_SPACE, SBT_RECORD_REGISTER_RAYGEN);

struct MaterialParams {
$RAYTRACING_MATERIAL_STRUCT
};

$RAYTRACING_SCENE_SHADER_PARAMS

VertexAttributesOutput fetch_vertex_attributes(float2 bary) {
    uint index = uint3(
        indices_buffer[drawable_sbt_record.index_offset + PrimitiveIndex()],
        indices_buffer[drawable_sbt_record.index_offset + PrimitiveIndex() + 1],
        indices_buffer[drawable_sbt_record.index_offset + PrimitiveIndex() + 2]
    );

    float3 position = float3(0.0);
#if (VERTEX_ATTRIBUTES_IN & VA_TYPE_POSITION) != 0
    float3 position0 = float3(
        positions_buffer[drawable_sbt_record.position_offset + index.x],
        positions_buffer[drawable_sbt_record.position_offset + index.x + 1],
        positions_buffer[drawable_sbt_record.position_offset + index.x + 2]
    );
    float3 position1 = float3(
        positions_buffer[drawable_sbt_record.position_offset + index.y],
        positions_buffer[drawable_sbt_record.position_offset + index.y + 1],
        positions_buffer[drawable_sbt_record.position_offset + index.y + 2]
    );
    float3 position2 = float3(
        positions_buffer[drawable_sbt_record.position_offset + index.z],
        positions_buffer[drawable_sbt_record.position_offset + index.z + 1],
        positions_buffer[drawable_sbt_record.position_offset + index.z + 2]
    );
    position = position0 + (position1 - position0) * bary.x + (position2 - position0) * bary.y;
#endif

    float3 normal = float3(0.0, 0.0, 1.0);
#if (VERTEX_ATTRIBUTES_IN & VA_TYPE_NORMAL) != 0
    float3 normal0 = float3(
        normals_buffer[drawable_sbt_record.normal_offset + index.x],
        normals_buffer[drawable_sbt_record.normal_offset + index.x + 1],
        normals_buffer[drawable_sbt_record.normal_offset + index.x + 2]
    );
    float3 normal1 = float3(
        normals_buffer[drawable_sbt_record.normal_offset + index.y],
        normals_buffer[drawable_sbt_record.normal_offset + index.y + 1],
        normals_buffer[drawable_sbt_record.normal_offset + index.y + 2]
    );
    float3 normal2 = float3(
        normals_buffer[drawable_sbt_record.normal_offset + index.z],
        normals_buffer[drawable_sbt_record.normal_offset + index.z + 1],
        normals_buffer[drawable_sbt_record.normal_offset + index.z + 2]
    );
    normal = normal0 + (normal1 - normal0) * bary.x + (normal2 - normal0) * bary.y;
#endif

    float4 tangent = float4(1.0, 0.0, 0.0, 1.0);
#if (VERTEX_ATTRIBUTES_IN & VA_TYPE_TANGENT) != 0
    float4 tangent0 = float4(
        tangents_buffer[drawable_sbt_record.tangent_offset + index.x],
        tangents_buffer[drawable_sbt_record.tangent_offset + index.x + 1],
        tangents_buffer[drawable_sbt_record.tangent_offset + index.x + 2],
        tangents_buffer[drawable_sbt_record.tangent_offset + index.x + 2]
    );
    float4 tangent1 = float4(
        tangents_buffer[drawable_sbt_record.tangent_offset + index.y],
        tangents_buffer[drawable_sbt_record.tangent_offset + index.y + 1],
        tangents_buffer[drawable_sbt_record.tangent_offset + index.y + 2],
        tangents_buffer[drawable_sbt_record.tangent_offset + index.y + 2]
    );
    float4 tangent2 = float4(
        tangents_buffer[drawable_sbt_record.tangent_offset + index.z],
        tangents_buffer[drawable_sbt_record.tangent_offset + index.z + 1],
        tangents_buffer[drawable_sbt_record.tangent_offset + index.z + 2],
        tangents_buffer[drawable_sbt_record.tangent_offset + index.z + 2]
    );
    tangent = tangent0 + (tangent1 - tangent0) * bary.x + (tangent2 - tangent0) * bary.y;
#endif

    float3 color = float3(0.0);
#if (VERTEX_ATTRIBUTES_IN & VA_TYPE_COLOR) != 0
    float3 color0 = float3(
        colors_buffer[drawable_sbt_record.color_offset + index.x],
        colors_buffer[drawable_sbt_record.color_offset + index.x + 1],
        colors_buffer[drawable_sbt_record.color_offset + index.x + 2]
    );
    float3 color1 = float3(
        colors_buffer[drawable_sbt_record.color_offset + index.y],
        colors_buffer[drawable_sbt_record.color_offset + index.y + 1],
        colors_buffer[drawable_sbt_record.color_offset + index.y + 2]
    );
    float3 color2 = float3(
        colors_buffer[drawable_sbt_record.color_offset + index.x],
        colors_buffer[drawable_sbt_record.color_offset + index.x + 1],
        colors_buffer[drawable_sbt_record.color_offset + index.x + 2]
    );
    color = color0 + (color1 - color0) * bary.x + (color2 - color0) * bary.y;
#endif

    float2 texcoord = float2(0.0);
#if (VERTEX_ATTRIBUTES_IN & VA_TYPE_TEXCOORD) != 0
    float texcoord_0 = float2(
        texcoords_buffer[drawable_sbt_record.texcoord_offset + index.x],
        texcoords_buffer[drawable_sbt_record.texcoord_offset + index.x + 1]
    );
    float texcoord_1 = float2(
        texcoords_buffer[drawable_sbt_record.texcoord_offset + index.y],
        texcoords_buffer[drawable_sbt_record.texcoord_offset + index.y + 1]
    );
    float texcoord_2 = float2(
        texcoords_buffer[drawable_sbt_record.texcoord_offset + index.z],
        texcoords_buffer[drawable_sbt_record.texcoord_offset + index.z + 1]
    );
    texcoord = texcoord_0 + (texcoord_1 - texcoord_0) * bary.x + (texcoord_2 - texcoord_0) * bary.y;
#endif

    float2 texcoord2 = float2(0.0);
#if (VERTEX_ATTRIBUTES_IN & VA_TYPE_TEXCOORD2) != 0
    float texcoord2_0 = float2(
        texcoords2_buffer[drawable_sbt_record.texcoord2_offset + index.x],
        texcoords2_buffer[drawable_sbt_record.texcoord2_offset + index.x + 1]
    );
    float texcoord2_1 = float2(
        texcoords2_buffer[drawable_sbt_record.texcoord2_offset + index.y],
        texcoords2_buffer[drawable_sbt_record.texcoord2_offset + index.y + 1]
    );
    float texcoord2_2 = float2(
        texcoords2_buffer[drawable_sbt_record.texcoord2_offset + index.z],
        texcoords2_buffer[drawable_sbt_record.texcoord2_offset + index.z + 1]
    );
    texcoord2 = texcoord2_0 + (texcoord2_1 - texcoord2_0) * bary.x + (texcoord2_2 - texcoord2_0) * bary.y;
#endif

    VertexAttributesOutput vert;
    vert.position_world = mul(ObjectToWorld3x4(), float4(position, 1.0));
    vert.history_position_world = mul(history_transforms_buffer[drawable_sbt_record.drawable_index], float4(position, 1.0)).xyz;
    vert.normal_world = normalize(mul(WorldToObject4x3(), normal).xyz);
    vert.tangent_world = normalize(mul(ObjectToWorld3x4(), float4(tangent.xyz, 0.0)).xyz);
    vert.bitangent_world = normalize(cross(vert.normal_world, vert.tangent_world)) * tangent.w;
    vert.color = color;
    vert.texcoord = texcoord;
    vert.texcoord2 = texcoord2;
    return vert;
}

SurfaceData material_function(VertexAttributesOutput vertex) {
    SurfaceData surface = surface_data_default();
    MaterialParams PARAM = material_params.Load<MaterialParams>(drawable_sbt_record.material_offset);

    $MATERIAL_FUNCTION

    return surface;
}
