#include "../core/vertex_attributes.hlsl"

VertexAttributesOutput screen_triangle_vs(uint vertex_id: SV_VertexID) {
    VertexAttributesOutput vout;
    float2 uv = float2((vertex_id & 1) << 1, vertex_id & 2);
    vout.sv_position = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 1.0, 1.0);
#if (VERTEX_ATTRIBUTES_OUT & VA_TYPE_POSITION) != 0
    vout.position_world = vout.sv_position;
#endif
#if (VERTEX_ATTRIBUTES_OUT & VA_TYPE_TEXCOORD) != 0
    vout.texcoord = uv;
#endif
    return vout;
}
