#pragma once

#include "../material.hlsl"

$GRAPHICS_MATERIAL_SHADER_PARAMS

SurfaceData material_function(VertexAttributesOutput vertex) {
    SurfaceData surface;
    surface.base_color = 0.5;
    surface.f0_color = 0.04;
    surface.f90_color = 1.0;
    surface.normal_map_value = float3(0.5, 0.5, 1.0);
    surface.roughness = 0.5;
    surface.anisotropy = 1.0;
    surface.ior = 1.5;

    $MATERIAL_FUNCTION

    return surface;
}