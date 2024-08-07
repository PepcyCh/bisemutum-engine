#pragma once

#include "../runtime/asset.hpp"
#include "../graphics/material.hpp"

namespace bi {

struct MaterialAsset final {
    static constexpr std::string_view asset_type_name = "Material";

    static auto load(Dyn<rt::IFile>::Ref file) -> rt::AssetAny;

    auto save(Dyn<rt::IFile>::Ref file) const -> void;

    gfx::Material material;

    rt::AssetId referenced_material = rt::AssetId::invalid;
    std::vector<std::pair<std::string, rt::AssetId>> referenced_textures;
};

}
