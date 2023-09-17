#pragma once

$GRAPHICS_CAMERA_SHADER_PARAMS

float3 camera_position_world() {
    return matrix_inv_view[3].xyz;
}
