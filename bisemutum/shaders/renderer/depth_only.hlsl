#include <bisemutum/shaders/core/vertex_attributes.hlsl>

#include <bisemutum/shaders/core/shader_params/camera.hlsl>
#include <bisemutum/shaders/core/shader_params/material.hlsl>
#include <bisemutum/shaders/core/shader_params/fragment.hlsl>

void depth_only_fs(VertexAttributesOutput fin) {
#ifdef MATERIAL_BLEND_MODE_ALPHA_TEST
    SurfaceData surface = material_function(fin);
    if (surface.opacity < ALPHA_TEST_OPACITY_THRESHOLD) {
        discard;
    }
#endif
}
