#pragma once

$CAMERA_SHADER_PARAMS

#ifndef NO_CAMERA_SHADER_PARAMS

float3 camera_position_world() {
    return float3(matrix_inv_view[0].w, matrix_inv_view[1].w, matrix_inv_view[2].w);
}

#endif
