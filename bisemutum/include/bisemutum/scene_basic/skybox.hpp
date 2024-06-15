#pragma once

#include <string_view>

#include "../prelude/idiom.hpp"
#include "../math/math.hpp"
#include "../runtime/scene.hpp"
#include "texture.hpp"

namespace bi {

struct SkyboxComponent final {
    static constexpr std::string_view component_type_name = "SkyboxComponent";

    rt::TAssetPtr<TextureAsset> texture;
    float strength = 1.0f;
    float3 color = float3{1.0f};

    float diffuse_strength = 1.0f;
    float specular_strength = 1.0f;
};
BI_SREFL(
    type(SkyboxComponent),
    field(texture),
    field(strength, RangeF{}),
    field(color, Color{}),
    field(diffuse_strength, RangeF{0.0f, 1.0f}),
    field(specular_strength, RangeF{0.0f, 1.0f})
)

struct SkyboxSystem final : PImpl<SkyboxSystem> {
    struct Impl;

    SkyboxSystem();

    auto init_on(Ref<rt::Scene> scene) -> void;
    auto update() -> void;

    struct SkyboxInfo final {
        Ptr<gfx::Texture> tex;
        float3 color;
        float diffuse_strength;
        float specular_strength;
        float4x4 transform;
    };
    auto current_skybox() -> SkyboxInfo&;
};

}
