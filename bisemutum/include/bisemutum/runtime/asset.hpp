#pragma once

#include <any>
#include <string>

#include "../prelude/ref.hpp"

namespace bi::rt {

template <typename T>
concept TAsset = requires () {
    { T::type_name } -> std::same_as<std::string_view const&>;
    { T::load(std::declval<std::string_view>()) } -> std::same_as<std::any>;
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
    auto load(std::string_view type) const -> std::any*;

    std::string asset_path;
};

template <TAsset Asset>
struct TAssetPtr final {
    auto asset() -> Ptr<Asset> { return asset_; }
    auto asset() const -> CPtr<Asset> { return asset_; }

    auto state() const -> AssetState {
        return asset_ptr_.state();
    }
    auto load() -> void {
        asset_ = std::any_cast<Asset>(asset_ptr_.load(Asset::type_name));
    }

private:
    Ptr<Asset> asset_;
    AssetPtr asset_ptr_;
};

}
