#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "../utils/serde.hpp"

namespace bi {

using float1 = float;
using float2 = glm::vec2;
using float3 = glm::vec3;
using float4 = glm::vec4;

using int1 = int;
using int2 = glm::ivec2;
using int3 = glm::ivec3;
using int4 = glm::ivec4;

using uint = uint32_t;
using uint1 = uint32_t;
using uint2 = glm::uvec2;
using uint3 = glm::uvec3;
using uint4 = glm::uvec4;

using bool1 = bool;
using bool2 = glm::bvec2;
using bool3 = glm::bvec3;
using bool4 = glm::bvec4;

using float2x2 = glm::mat2;
using float2x3 = glm::mat3x2;
using float2x4 = glm::mat4x2;
using float3x2 = glm::mat2x3;
using float3x3 = glm::mat3;
using float3x4 = glm::mat4x3;
using float4x2 = glm::mat2x4;
using float4x3 = glm::mat3x4;
using float4x4 = glm::mat4;

using quaternion = glm::quat;

namespace math {

using namespace glm;

template <size_t C, size_t R>
constexpr auto mul(glm::vec<R, float> const& vec, glm::mat<C, R, float> const& mat) -> glm::vec<C, float> {
    return vec * mat;
}
template <size_t C, size_t R>
constexpr auto mul(glm::mat<C, R, float> const& mat, glm::vec<C, float> const& vec) -> glm::vec<R, float> {
    return mat * vec;
}

inline auto lerp(float a, float b, float t) -> float {
    return glm::mix(a, b, t);
}
template <size_t N>
auto lerp(glm::vec<N, float> const& a, glm::vec<N, float> const& b, float t) -> glm::vec<N, float> {
    return glm::mix(a, b, t);
}
template <size_t N>
auto lerp(glm::vec<N, float> const& a, glm::vec<N, float> const& b, glm::vec<N, float> const& t) -> glm::vec<N, float> {
    return glm::mix(a, b, t);
}

template <typename T>
constexpr auto saturate(T const& v) -> T {
    return glm::clamp(v, T(0.0f), T(1.0f));
}

template <typename T>
constexpr auto select(bool cond, T const& t, T const& f) {
    return cond ? t : f;
}

} // namespace math

namespace serde {

template <>
inline auto to_value<float2>(Value& v, float2 const& o) -> void {
    to_value(v, std::array{o.x, o.y});
}
template <>
inline auto from_value<float2>(Value const& v, float2& o) -> void {
    auto arr = v.get<std::array<float, 2>>();
    o = {arr[0], arr[1]};
}

template <>
inline auto to_value<float3>(Value& v, float3 const& o) -> void {
    to_value(v, std::array{o.x, o.y, o.z});
}
template <>
inline auto from_value<float3>(Value const& v, float3& o) -> void {
    auto arr = v.get<std::array<float, 3>>();
    o = {arr[0], arr[1], arr[2]};
}

template <>
inline auto to_value<float4>(Value& v, float4 const& o) -> void {
    to_value(v, std::array{o.x, o.y, o.z, o.w});
}
template <>
inline auto from_value<float4>(Value const& v, float4& o) -> void {
    auto arr = v.get<std::array<float, 4>>();
    o = {arr[0], arr[1], arr[2], arr[3]};
}

template <>
inline auto to_value<int2>(Value& v, int2 const& o) -> void {
    to_value(v, std::array{o.x, o.y});
}
template <>
inline auto from_value<int2>(Value const& v, int2& o) -> void {
    auto arr = v.get<std::array<int, 2>>();
    o = {arr[0], arr[1]};
}

template <>
inline auto to_value<int3>(Value& v, int3 const& o) -> void {
    to_value(v, std::array{o.x, o.y, o.z});
}
template <>
inline auto from_value<int3>(Value const& v, int3& o) -> void {
    auto arr = v.get<std::array<int, 3>>();
    o = {arr[0], arr[1], arr[2]};
}

template <>
inline auto to_value<int4>(Value& v, int4 const& o) -> void {
    to_value(v, std::array{o.x, o.y, o.z, o.w});
}
template <>
inline auto from_value<int4>(Value const& v, int4& o) -> void {
    auto arr = v.get<std::array<int, 4>>();
    o = {arr[0], arr[1], arr[2], arr[3]};
}

template <>
inline auto to_value<uint2>(Value& v, uint2 const& o) -> void {
    to_value(v, std::array{o.x, o.y});
}
template <>
inline auto from_value<uint2>(Value const& v, uint2& o) -> void {
    auto arr = v.get<std::array<uint32_t, 2>>();
    o = {arr[0], arr[1]};
}

template <>
inline auto to_value<uint3>(Value& v, uint3 const& o) -> void {
    to_value(v, std::array{o.x, o.y, o.z});
}
template <>
inline auto from_value<uint3>(Value const& v, uint3& o) -> void {
    auto arr = v.get<std::array<uint32_t, 3>>();
    o = {arr[0], arr[1], arr[2]};
}

template <>
inline auto to_value<uint4>(Value& v, uint4 const& o) -> void {
    to_value(v, std::array{o.x, o.y, o.z, o.w});
}
template <>
inline auto from_value<uint4>(Value const& v, uint4& o) -> void {
    auto arr = v.get<std::array<uint32_t, 4>>();
    o = {arr[0], arr[1], arr[2], arr[3]};
}

template <>
inline auto to_value<quaternion>(Value& v, quaternion const& o) -> void {
    to_value(v, std::array{o.w, o.x, o.y, o.z});
}
template <>
inline auto from_value<quaternion>(Value const& v, quaternion& o) -> void {
    auto arr = v.get<std::array<float, 4>>();
    o = {arr[0], arr[1], arr[2], arr[3]};
}

} // namespace serde

} // namespace bi
