#include <bisemutum/math/math.hpp>

namespace bi::math {

auto perspective_reverse_z(float fovy, float aspect, float near, float far) -> float4x4 {
    auto mat = perspective(fovy, aspect, near, far);
    auto inv = 1.0f / (far - near);
    mat[2][2] = near * inv;
    mat[3][2] = near * far * inv;
    return mat;
}

auto ortho_reverse_z(float left, float right, float bottom, float top, float near, float far) -> float4x4 {
    auto mat = ortho(left, right, bottom, top, near, far);
    auto inv = 1.0f / (far - near);
    mat[2][2] = inv;
    mat[3][2] = far * inv;
    return mat;
}

}
