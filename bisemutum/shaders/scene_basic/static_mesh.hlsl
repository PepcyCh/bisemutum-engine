#pragma once

#include "../core/vertex_processer.hlsl"

#include "../core/shader_params/camera.hlsl"
#include "../core/shader_params/mesh.hlsl"

VertexAttributesOutput static_mesh_vs(VertexAttributes vin) {
    VertexAttributesOutput vout;
    vout.position_world = mul(matrix_object_to_world, float4(vin.position, 1.0));
    vout.sv_position = mul(matrix_proj_view, float4(vout.position_world, 1.0));
    vout.normal_world = normalize(mul(matrix_world_to_object_transposed, float4(vin.normal, 0.0)));
    vout.tangent_world = normalize(mul(matrix_object_to_world, float4(vin.tangent.xyz, 0.0)));
    vout.bitangent_world = normalize(cross(vout.normal_world, vout.tangent_world)) * vin.tangent_world.w;
    vout.texcoord0 = vin.texcoord0;
    return vout;
}
