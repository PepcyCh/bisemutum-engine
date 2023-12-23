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
    float3 color = float3{1.0f};
};
BI_SREFL(
    type(SkyboxComponent),
    field(texture),
    field(color, Color{})
)

struct SkyboxSystem final : PImpl<SkyboxSystem> {
    struct Impl;

    SkyboxSystem();
    ~SkyboxSystem();

    SkyboxSystem(SkyboxSystem&& rhs) noexcept;
    auto operator=(SkyboxSystem&& rhs) noexcept -> SkyboxSystem&;

    auto init_on(Ref<rt::Scene> scene) -> void;
    auto update() -> void;
};

}
