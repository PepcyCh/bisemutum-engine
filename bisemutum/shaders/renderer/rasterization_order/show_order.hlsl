#include <bisemutum/shaders/core/vertex_attributes.hlsl>

#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/fragment.hlsl>

float4 ro_show_order_fs(VertexAttributesOutput fin) : SV_Target {
    uint order;
    InterlockedAdd(counting[0], 1, order);
    float t = float(order & 0x3ffff) / 0x40000;
    return float4(t, t, t, 1.0);
}
