#include "../core/vertex_attributes.hlsl"

#include "../core/shader_params/camera.hlsl"
#include "../core/shader_params/mesh.hlsl"

VertexAttributesOutput static_mesh_vs(VertexAttributes vin) {
    VertexAttributesOutput vout;

    float3 position_world = mul(matrix_object_to_world, float4(vin.position, 1.0)).xyz;
    vout.sv_position = mul(matrix_proj_view, float4(position_world, 1.0));
#if (VERTEX_ATTRIBUTES_OUT & VA_TYPE_POSITION) != 0
    vout.position_world = position_world;
#endif
#if (VERTEX_ATTRIBUTES_OUT & VA_TYPE_HISTORY_POSITION) != 0
    vout.history_position_world = mul(history_matrix_object_to_world, float4(vin.position, 1.0)).xyz;
#endif

#if (VERTEX_ATTRIBUTES_OUT & VA_TYPE_NORMAL) != 0
#if (VERTEX_ATTRIBUTES_IN & VA_TYPE_NORMAL) != 0
    vout.normal_world = normalize(mul(matrix_world_to_object_transposed, float4(vin.normal, 0.0)).xyz);
#else
    vout.normal_world = float3(0.0, 0.0, 1.0);
#endif
#endif

#if (VERTEX_ATTRIBUTES_OUT & VA_TYPE_TANGENT) != 0
#if (VERTEX_ATTRIBUTES_IN & VA_TYPE_TANGENT) != 0
    vout.tangent_world = normalize(mul(matrix_object_to_world, float4(vin.tangent.xyz, 0.0)).xyz);
#else
    vout.tangent_world = float3(1.0, 0.0, 0.0);
#endif
    vout.bitangent_world = normalize(cross(vout.normal_world, vout.tangent_world)) * vin.tangent.w;
#endif

#if (VERTEX_ATTRIBUTES_OUT & VA_TYPE_COLOR) != 0
#if (VERTEX_ATTRIBUTES_IN & VA_TYPE_COLOR) != 0
    vout.color = vin.color;
#else
    vout.color = float3(0.0, 0.0, 0.0);
#endif
#endif

#if (VERTEX_ATTRIBUTES_OUT & VA_TYPE_TEXCOORD) != 0
#if (VERTEX_ATTRIBUTES_IN & VA_TYPE_TEXCOORD) != 0
    vout.texcoord = vin.texcoord;
#else
    vout.texcoord = float2(0.0, 0.0);
#endif
#endif

    return vout;
}
