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
ConstantBuffer<DrawableSbtData> drawable_sbt_record : register(SBT_RECORD_REGISTER_HIT, SBT_RECORD_SPACE);

struct MaterialParams {
$RAYTRACING_MATERIAL_STRUCT
};

$RAYTRACING_SCENE_SHADER_PARAMS

VertexAttributesOutput fetch_vertex_attributes(float2 bary) {
    uint3 index = uint3(
        indices_buffer[NonUniformResourceIndex(drawable_sbt_record.index_offset + 3 * PrimitiveIndex())],
        indices_buffer[NonUniformResourceIndex(drawable_sbt_record.index_offset + 3 * PrimitiveIndex() + 1)],
        indices_buffer[NonUniformResourceIndex(drawable_sbt_record.index_offset + 3 * PrimitiveIndex() + 2)]
    );

    float3 position = 0.0;
#if (VERTEX_ATTRIBUTES_IN & VA_TYPE_POSITION) != 0
    float3 position0 = float3(
        positions_buffer[NonUniformResourceIndex(drawable_sbt_record.position_offset + 3 * index.x)],
        positions_buffer[NonUniformResourceIndex(drawable_sbt_record.position_offset + 3 * index.x + 1)],
        positions_buffer[NonUniformResourceIndex(drawable_sbt_record.position_offset + 3 * index.x + 2)]
    );
    float3 position1 = float3(
        positions_buffer[NonUniformResourceIndex(drawable_sbt_record.position_offset + 3 * index.y)],
        positions_buffer[NonUniformResourceIndex(drawable_sbt_record.position_offset + 3 * index.y + 1)],
        positions_buffer[NonUniformResourceIndex(drawable_sbt_record.position_offset + 3 * index.y + 2)]
    );
    float3 position2 = float3(
        positions_buffer[NonUniformResourceIndex(drawable_sbt_record.position_offset + 3 * index.z)],
        positions_buffer[NonUniformResourceIndex(drawable_sbt_record.position_offset + 3 * index.z + 1)],
        positions_buffer[NonUniformResourceIndex(drawable_sbt_record.position_offset + 3 * index.z + 2)]
    );
    position = position0 + (position1 - position0) * bary.x + (position2 - position0) * bary.y;
#endif

    float3 normal = float3(0.0, 0.0, 1.0);
#if (VERTEX_ATTRIBUTES_IN & VA_TYPE_NORMAL) != 0
    float3 normal0 = float3(
        normals_buffer[NonUniformResourceIndex(drawable_sbt_record.normal_offset + 3 * index.x)],
        normals_buffer[NonUniformResourceIndex(drawable_sbt_record.normal_offset + 3 * index.x + 1)],
        normals_buffer[NonUniformResourceIndex(drawable_sbt_record.normal_offset + 3 * index.x + 2)]
    );
    float3 normal1 = float3(
        normals_buffer[NonUniformResourceIndex(drawable_sbt_record.normal_offset + 3 * index.y)],
        normals_buffer[NonUniformResourceIndex(drawable_sbt_record.normal_offset + 3 * index.y + 1)],
        normals_buffer[NonUniformResourceIndex(drawable_sbt_record.normal_offset + 3 * index.y + 2)]
    );
    float3 normal2 = float3(
        normals_buffer[NonUniformResourceIndex(drawable_sbt_record.normal_offset + 3 * index.z)],
        normals_buffer[NonUniformResourceIndex(drawable_sbt_record.normal_offset + 3 * index.z + 1)],
        normals_buffer[NonUniformResourceIndex(drawable_sbt_record.normal_offset + 3 * index.z + 2)]
    );
    normal = normal0 + (normal1 - normal0) * bary.x + (normal2 - normal0) * bary.y;
#endif

    float4 tangent = float4(1.0, 0.0, 0.0, 1.0);
#if (VERTEX_ATTRIBUTES_IN & VA_TYPE_TANGENT) != 0
    float4 tangent0 = float4(
        tangents_buffer[NonUniformResourceIndex(drawable_sbt_record.tangent_offset + 4 * index.x)],
        tangents_buffer[NonUniformResourceIndex(drawable_sbt_record.tangent_offset + 4 * index.x + 1)],
        tangents_buffer[NonUniformResourceIndex(drawable_sbt_record.tangent_offset + 4 * index.x + 2)],
        tangents_buffer[NonUniformResourceIndex(drawable_sbt_record.tangent_offset + 4 * index.x + 3)]
    );
    float4 tangent1 = float4(
        tangents_buffer[NonUniformResourceIndex(drawable_sbt_record.tangent_offset + 4 * index.y)],
        tangents_buffer[NonUniformResourceIndex(drawable_sbt_record.tangent_offset + 4 * index.y + 1)],
        tangents_buffer[NonUniformResourceIndex(drawable_sbt_record.tangent_offset + 4 * index.y + 2)],
        tangents_buffer[NonUniformResourceIndex(drawable_sbt_record.tangent_offset + 4 * index.y + 3)]
    );
    float4 tangent2 = float4(
        tangents_buffer[NonUniformResourceIndex(drawable_sbt_record.tangent_offset + 4 * index.z)],
        tangents_buffer[NonUniformResourceIndex(drawable_sbt_record.tangent_offset + 4 * index.z + 1)],
        tangents_buffer[NonUniformResourceIndex(drawable_sbt_record.tangent_offset + 4 * index.z + 2)],
        tangents_buffer[NonUniformResourceIndex(drawable_sbt_record.tangent_offset + 4 * index.z + 3)]
    );
    tangent = tangent0 + (tangent1 - tangent0) * bary.x + (tangent2 - tangent0) * bary.y;
#endif

    float3 color = 0.0;
#if (VERTEX_ATTRIBUTES_IN & VA_TYPE_COLOR) != 0
    float3 color0 = float3(
        colors_buffer[NonUniformResourceIndex(drawable_sbt_record.color_offset + 3 * index.x)],
        colors_buffer[NonUniformResourceIndex(drawable_sbt_record.color_offset + 3 * index.x + 1)],
        colors_buffer[NonUniformResourceIndex(drawable_sbt_record.color_offset + 3 * index.x + 2)]
    );
    float3 color1 = float3(
        colors_buffer[NonUniformResourceIndex(drawable_sbt_record.color_offset + 3 * index.y)],
        colors_buffer[NonUniformResourceIndex(drawable_sbt_record.color_offset + 3 * index.y + 1)],
        colors_buffer[NonUniformResourceIndex(drawable_sbt_record.color_offset + 3 * index.y + 2)]
    );
    float3 color2 = float3(
        colors_buffer[NonUniformResourceIndex(drawable_sbt_record.color_offset + 3 * index.x)],
        colors_buffer[NonUniformResourceIndex(drawable_sbt_record.color_offset + 3 * index.x + 1)],
        colors_buffer[NonUniformResourceIndex(drawable_sbt_record.color_offset + 3 * index.x + 2)]
    );
    color = color0 + (color1 - color0) * bary.x + (color2 - color0) * bary.y;
#endif

    float2 texcoord = 0.0;
#if (VERTEX_ATTRIBUTES_IN & VA_TYPE_TEXCOORD) != 0
    float2 texcoord_0 = float2(
        texcoords_buffer[NonUniformResourceIndex(drawable_sbt_record.texcoord_offset + 2 * index.x)],
        texcoords_buffer[NonUniformResourceIndex(drawable_sbt_record.texcoord_offset + 2 * index.x + 1)]
    );
    float2 texcoord_1 = float2(
        texcoords_buffer[NonUniformResourceIndex(drawable_sbt_record.texcoord_offset + 2 * index.y)],
        texcoords_buffer[NonUniformResourceIndex(drawable_sbt_record.texcoord_offset + 2 * index.y + 1)]
    );
    float2 texcoord_2 = float2(
        texcoords_buffer[NonUniformResourceIndex(drawable_sbt_record.texcoord_offset + 2 * index.z)],
        texcoords_buffer[NonUniformResourceIndex(drawable_sbt_record.texcoord_offset + 2 * index.z + 1)]
    );
    texcoord = texcoord_0 + (texcoord_1 - texcoord_0) * bary.x + (texcoord_2 - texcoord_0) * bary.y;
#endif

    float2 texcoord2 = 0.0;
#if (VERTEX_ATTRIBUTES_IN & VA_TYPE_TEXCOORD2) != 0
    float2 texcoord2_0 = float2(
        texcoords2_buffer[NonUniformResourceIndex(drawable_sbt_record.texcoord2_offset + 2 * index.x)],
        texcoords2_buffer[NonUniformResourceIndex(drawable_sbt_record.texcoord2_offset + 2 * index.x + 1)]
    );
    float2 texcoord2_1 = float2(
        texcoords2_buffer[NonUniformResourceIndex(drawable_sbt_record.texcoord2_offset + 2 * index.y)],
        texcoords2_buffer[NonUniformResourceIndex(drawable_sbt_record.texcoord2_offset + 2 * index.y + 1)]
    );
    float2 texcoord2_2 = float2(
        texcoords2_buffer[NonUniformResourceIndex(drawable_sbt_record.texcoord2_offset + 2 * index.z)],
        texcoords2_buffer[NonUniformResourceIndex(drawable_sbt_record.texcoord2_offset + 2 * index.z + 1)]
    );
    texcoord2 = texcoord2_0 + (texcoord2_1 - texcoord2_0) * bary.x + (texcoord2_2 - texcoord2_0) * bary.y;
#endif

    VertexAttributesOutput vert;
    vert.position_world = mul(ObjectToWorld3x4(), float4(position, 1.0)).xyz;
    vert.history_position_world = mul(
        history_transforms_buffer[NonUniformResourceIndex(drawable_sbt_record.drawable_index)],
        float4(position, 1.0)
    ).xyz;
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
