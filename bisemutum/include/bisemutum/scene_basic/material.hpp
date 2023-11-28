#pragma once

#include "../runtime/asset.hpp"
#include "../graphics/material.hpp"

namespace bi {

struct MaterialAsset final {
    static constexpr std::string_view asset_type_name = "Material";

    static auto load(Dyn<rt::IFile>::Ref file) -> rt::AssetAny;

    static auto save(Dyn<rt::IFile>::Ref file, rt::AssetAny const& asset) -> void;

    gfx::Material material;
    // std::string_view
};

}
