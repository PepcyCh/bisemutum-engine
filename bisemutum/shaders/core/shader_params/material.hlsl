#pragma once

#include "../material.hlsl"
#include "../vertex_attributes.hlsl"

$GRAPHICS_MATERIAL_SHADER_PARAMS

SurfaceData material_function(VertexAttributesOutput vertex) {
    SurfaceData surface = surface_data_default();

    $MATERIAL_FUNCTION

    return surface;
}