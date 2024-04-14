#pragma once

float3 position_view_from_depth(float2 uv, float depth, float4x4 matrix_inv_proj) {
    float4 pos_ndc = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth, 1.0);
    float4 pos_view = mul(matrix_inv_proj, pos_ndc);
    pos_view /= pos_view.w;
    return pos_view.xyz;
}

float3 position_world_from_depth(float2 uv, float depth, float4x4 matrix_inv_proj, float4x4 matrix_inv_view) {
    float4 pos_ndc = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth, 1.0);
    float4 pos_view = mul(matrix_inv_proj, pos_ndc);
    return mul(matrix_inv_view, pos_view / pos_view.w).xyz;
}
