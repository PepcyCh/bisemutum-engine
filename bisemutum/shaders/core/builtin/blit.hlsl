#include <bisemutum/shaders/core/vertex_attributes.hlsl>

#include <bisemutum/shaders/core/shader_params/fragment.hlsl>

#ifndef BLIT_DEPTH
float4 blit_fs(VertexAttributesOutput fin) : SV_Target {
    return src_texture.SampleLevel(sampler_src_texture, fin.texcoord, 0);
}
#else
float blit_fs(VertexAttributesOutput fin) : SV_Depth {
    return src_texture.SampleLevel(sampler_src_texture, fin.texcoord, 0).x;
}
#endif
