#pragma once

#include <memory>
#include <string>

#include "defines.hpp"
#include "core/utils.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

enum class SamplerFilterMode : uint8_t {
    eNearest,
    eLinear,
};

enum class SamplerMipmapMode : uint8_t {
    eNearest,
    eLinear,
};

enum class SamplerAddressMode : uint8_t {
    eRepeat,
    eMirrorRepeat,
    eClampToEdge,
    eClampToBorder,
    eMirrorClampToEdge,
};

enum class SamplerBorderColor : uint8_t {
    TransparentFloat,
    TransparentInt,
    BlackFloat,
    BlackInt,
    WhiteFloat,
    WhiteInt,
};

struct SamplerDesc {
    SamplerFilterMode mag_filter = SamplerFilterMode::eNearest;
    SamplerFilterMode min_filter = SamplerFilterMode::eNearest;
    SamplerMipmapMode mipmap_mode = SamplerMipmapMode::eNearest;
    SamplerAddressMode address_mode_u = SamplerAddressMode::eRepeat;
    SamplerAddressMode address_mode_v = SamplerAddressMode::eRepeat;
    SamplerAddressMode address_mode_w = SamplerAddressMode::eRepeat;
    SamplerBorderColor border_color = SamplerBorderColor::TransparentFloat;
    bool compare_enabled = false;
    CompareOp compare_op = CompareOp::eNever;
    float anisotropy = 0.0f;
    float lod_bias = 0.0f;
    float lod_min = 0.0f;
    float lod_max = 1000.0f;

    bool operator==(const SamplerDesc &rhs) const = default;
};

class Sampler {
public:
    virtual ~Sampler() = default;

protected:
    Sampler() = default;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

template <>
struct std::hash<bismuth::gfx::SamplerDesc> {
    size_t operator()(const bismuth::gfx::SamplerDesc &v) const noexcept {
        return bismuth::Hash(
            v.mag_filter, v.min_filter, v.mipmap_mode,
            v.address_mode_u, v.address_mode_v, v.address_mode_w, v.border_color,
            v.compare_enabled, v.compare_op, v.anisotropy, v.lod_bias, v.lod_min, v.lod_max
        );
    }
};
