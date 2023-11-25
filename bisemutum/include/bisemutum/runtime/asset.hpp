#pragma once

#include "vfs.hpp"
#include "../utils/serde.hpp"
#include "../prelude/ref.hpp"

namespace bi::rt {

using AssetAny = aa::any_with<aa::type_info, aa::move>;

template <typename T>
concept TAsset = requires () {
    { T::asset_type_name } -> std::same_as<std::string_view const&>;
    { T::load(std::declval<Dyn<IFile>::Ref>()) } -> std::same_as<AssetAny>;
};

enum class AssetState : uint8_t {
    not_loaded,
    loaded,
    loading,
    error,
    not_found,
};

struct AssetPtr final {
    auto state() const -> AssetState;
    auto load(std::string_view type) const -> AssetAny*;

    std::string asset_path;
};

template <TAsset Asset>
struct TAssetPtr final {
    TAssetPtr() = default;
    TAssetPtr(std::string_view path) {
        asset_ = nullptr;
        asset_ptr_.asset_path = path;
    }
    TAssetPtr(std::string&& path) {
        asset_ = nullptr;
        asset_ptr_.asset_path = std::move(path);
    }

    auto asset() -> Ptr<Asset> { return asset_; }
    auto asset() const -> Ptr<Asset> { return asset_; }

    auto state() const -> AssetState {
        return asset_ptr_.state();
    }
    auto load() const -> void {
        asset_ = aa::any_cast<Asset>(asset_ptr_.load(Asset::asset_type_name));
    }

    auto empty() const -> bool {
        return asset_ptr_.asset_path.empty();
    }

    static auto to_value(serde::Value &v, rt::TAssetPtr<Asset> const& o) -> void {
        v["asset_path"] = o.asset_ptr_.asset_path;
    }
    static auto from_value(serde::Value const& v, rt::TAssetPtr<Asset>& o) -> void {
        o.asset_ = nullptr;
        o.asset_ptr_.asset_path = v["asset_path"].get<std::string>();
    }

private:
    mutable Ptr<Asset> asset_;
    AssetPtr asset_ptr_;
};

}
