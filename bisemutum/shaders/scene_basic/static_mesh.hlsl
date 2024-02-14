#include "../core/vertex_attributes.hlsl"

#include "../core/shader_params/camera.hlsl"
#include "../core/shader_params/mesh.hlsl"

VertexAttributesOutput static_mesh_vs(VertexAttributes vin) {
    VertexAttributesOutput vout;
#if (VERTEX_ATTRIBUTES_TYPE & VERTEX_ATTRIBUTES_TYPE_POSITION) != 0
    vout.position_world = mul(matrix_object_to_world, float4(vin.position, 1.0));
    vout.sv_position = mul(matrix_proj_view, float4(vout.position_world, 1.0));
#else
    vout.sv_position = mul(matrix_proj_view, mul(matrix_object_to_world, float4(vin.position, 1.0)));
#endif
#if (VERTEX_ATTRIBUTES_TYPE & VERTEX_ATTRIBUTES_TYPE_HISTORY_POSITION) != 0
    vout.history_position_world = mul(history_matrix_object_to_world, float4(vin.position, 1.0)).xyz;
#endif
#if (VERTEX_ATTRIBUTES_TYPE & VERTEX_ATTRIBUTES_TYPE_NORMAL) != 0
    vout.normal_world = normalize(mul(matrix_world_to_object_transposed, float4(vin.normal, 0.0)).xyz);
#endif
#if (VERTEX_ATTRIBUTES_TYPE & VERTEX_ATTRIBUTES_TYPE_TANGENT) != 0
    vout.tangent_world = normalize(mul(matrix_object_to_world, float4(vin.tangent.xyz, 0.0)).xyz);
    vout.bitangent_world = normalize(cross(vout.normal_world, vout.tangent_world)) * vin.tangent.w;
#endif
#if (VERTEX_ATTRIBUTES_TYPE & VERTEX_ATTRIBUTES_TYPE_TEXCOORD) != 0
    vout.texcoord = vin.texcoord;
#endif
    return vout;
}
