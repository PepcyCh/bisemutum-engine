#pragma once

#include <memory>
#include <string>

#include "defines.hpp"
#include "../prelude/hash.hpp"

namespace bi::rhi {

enum class SamplerFilterMode : uint8_t {
    nearest,
    linear,
};

enum class SamplerMipmapMode : uint8_t {
    nearest,
    linear,
};

enum class SamplerAddressMode : uint8_t {
    repeat,
    mirror_repeat,
    clamp_to_edge,
    clamp_to_border,
    mirror_clamp_to_edge,
};

enum class SamplerBorderColor : uint8_t {
    transparent_float,
    transparent_int,
    black_float,
    black_int,
    white_float,
    white_int,
};

struct SamplerDesc final {
    SamplerFilterMode mag_filter = SamplerFilterMode::nearest;
    SamplerFilterMode min_filter = SamplerFilterMode::nearest;
    SamplerMipmapMode mipmap_mode = SamplerMipmapMode::nearest;
    SamplerAddressMode address_mode_u = SamplerAddressMode::repeat;
    SamplerAddressMode address_mode_v = SamplerAddressMode::repeat;
    SamplerAddressMode address_mode_w = SamplerAddressMode::repeat;
    SamplerBorderColor border_color = SamplerBorderColor::transparent_float;
    bool compare_enabled = false;
    CompareOp compare_op = CompareOp::never;
    float anisotropy = 0.0f;
    float lod_bias = 0.0f;
    float lod_min = 0.0f;
    float lod_max = 1000.0f;

    auto operator==(SamplerDesc const& rhs) const -> bool = default;
};

struct Sampler {
    virtual ~Sampler() = default;
};

}

template <>
struct std::hash<bi::rhi::SamplerDesc> final {
    size_t operator()(bi::rhi::SamplerDesc const& v) const noexcept {
        return bi::hash(
            v.mag_filter, v.min_filter, v.mipmap_mode,
            v.address_mode_u, v.address_mode_v, v.address_mode_w, v.border_color,
            v.compare_enabled, v.compare_op, v.anisotropy, v.lod_bias, v.lod_min, v.lod_max
        );
    }
};
