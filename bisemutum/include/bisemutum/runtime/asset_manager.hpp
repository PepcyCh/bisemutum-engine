#pragma once

#include <functional>

#include "asset.hpp"
#include "../prelude/idiom.hpp"

namespace bi::rt {

using AssetLoader = auto(Dyn<IFile>::Ref) -> AssetAny;

struct AssetManager final : PImpl<AssetManager> {
    struct Impl;

    AssetManager();
    ~AssetManager();

    template <TAsset Asset>
    auto register_asset() -> void {
        register_asset(Asset::asset_type_name, Asset::load);
    }

    auto state_of(std::string_view asset_path) -> AssetState;

private:
    auto register_asset(std::string_view type, std::function<AssetLoader> loader) -> void;

    friend AssetPtr;
    auto load_asset(std::string_view type, std::string_view asset_path) -> AssetAny*;
};

}