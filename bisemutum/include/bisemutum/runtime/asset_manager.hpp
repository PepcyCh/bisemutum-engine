#pragma once

#include <functional>

#include "asset.hpp"
#include "../prelude/idiom.hpp"
#include "../prelude/ref.hpp"

namespace bi::rt {

using AssetLoader = auto(Dyn<IFile>::Ref) -> AssetAny;
using AssetSaver = auto(Dyn<IFile>::Ref, AssetAny const&) -> void;

struct AssetMetadata final {
    uint64_t id;
    std::string type;
    std::string path;
};
BI_SREFL(type(AssetMetadata), field(id), field(type), field(path));

struct AssetManager final : PImpl<AssetManager> {
    struct Impl;

    AssetManager();

    struct AssetFunctions final {
        std::function<AssetLoader> loader;
        std::function<AssetSaver> saver;
    };

    auto initialize(Dyn<IFile>::Ref metadata_file) -> bool;

    template <TAsset Asset>
    auto register_asset() -> void {
        AssetFunctions functions{
            .loader = Asset::load,
            .saver = [](Dyn<IFile>::Ref file, AssetAny const& value) {
                aa::any_cast<Asset const&>(value).save(file);
            },
        };
        register_asset(Asset::asset_type_name, std::move(functions));
    }

    auto state_of(AssetId asset_id) -> AssetState;

    auto metadata_of(AssetId asset_id) const -> CPtr<AssetMetadata>;
    auto all_metadata_of_type(std::string_view type) const -> std::vector<CRef<AssetMetadata>>;

    auto asset_id_of(std::string_view path) -> AssetId;

    template <TAsset Asset>
    auto get_and_load_asset(std::string_view asset_path) -> std::pair<AssetId, Ptr<Asset>> {
        auto id = asset_id_of(asset_path);
        TAssetPtr<Asset> asset_ptr{id};
        asset_ptr.load();
        return {id, asset_ptr.asset()};
    }

    template <TAsset Asset>
    auto create_asset(std::string_view asset_path, Asset&& asset) -> std::pair<AssetId, Ref<Asset>> {
        auto [id, asset_ptr] = create_asset(Asset::asset_type_name, asset_path, AssetAny{std::move(asset)});
        return {id, Ptr<Asset>{aa::any_cast<Asset>(asset_ptr)}.value()};
    }

    auto save_all_assets(Dyn<IFile>::Ref metadata_file, bool force) -> void;

private:
    auto register_asset(std::string_view type, AssetFunctions&& functions) -> void;

    friend AssetPtr;
    auto load_asset(AssetId asset_id) -> AssetAny*;

    auto create_asset(
        std::string_view asset_type_name, std::string_view asset_path, AssetAny asset
    ) -> std::pair<AssetId, AssetAny*>;
};

}
