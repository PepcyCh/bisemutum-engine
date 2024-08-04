#include <bisemutum/shaders/core/shader_params/compute.hlsl>

[numthreads(1, 1, 1)]
void ro_initialize_cs() {
    counting[0] = 0;
}
