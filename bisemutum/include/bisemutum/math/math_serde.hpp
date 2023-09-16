#pragma once

#include "math.hpp"
#include "../utils/serde.hpp"

namespace bi::serde {

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

}
