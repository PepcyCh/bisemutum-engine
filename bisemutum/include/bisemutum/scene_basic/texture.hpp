#pragma once

#include <string>

#include "../runtime/asset.hpp"
#include "../graphics/resource.hpp"
#include "../graphics/sampler.hpp"

namespace bi {

struct TextureAsset final {
    static constexpr std::string_view asset_type_name = "Texture";

    static auto load(Dyn<rt::IFile>::Ref file) -> rt::AssetAny;

    gfx::Texture texture;
    Ptr<gfx::Sampler> sampler;
};

}
