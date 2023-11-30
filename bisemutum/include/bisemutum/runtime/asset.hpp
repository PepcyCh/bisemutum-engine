#pragma once

#include "vfs.hpp"
#include "../utils/serde.hpp"
#include "../prelude/ref.hpp"

namespace bi::rt {

using AssetAny = aa::any_with<aa::type_info, aa::move>;

enum class AssetId : uint64_t {
    invalid = static_cast<uint64_t>(-1),
};

template <typename T>
concept TAsset = requires (T v) {
    { T::asset_type_name } -> std::same_as<std::string_view const&>;
    { T::load(std::declval<Dyn<IFile>::Ref>()) } -> std::same_as<AssetAny>;
    { v.save(std::declval<Dyn<IFile>::Ref>()) } -> std::same_as<void>;
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
    auto load() const -> AssetAny*;

    AssetId asset_id = AssetId::invalid;
};

template <TAsset Asset>
struct TAssetPtr final {
    TAssetPtr() = default;
    TAssetPtr(AssetId asset_id) {
        asset_ = nullptr;
        asset_ptr_.asset_id = asset_id;
    }

    auto asset() -> Ptr<Asset> { return asset_; }
    auto asset() const -> Ptr<Asset> { return asset_; }

    auto state() const -> AssetState {
        return asset_ptr_.state();
    }
    auto load() const -> void {
        asset_ = aa::any_cast<Asset>(asset_ptr_.load());
    }

    auto empty() const -> bool {
        return asset_ptr_.asset_id == AssetId::invalid;
    }

    static auto to_value(serde::Value &v, rt::TAssetPtr<Asset> const& o) -> void {
        v["asset_id"] = static_cast<serde::Value::Integer>(o.asset_ptr_.asset_id);
    }
    static auto from_value(serde::Value const& v, rt::TAssetPtr<Asset>& o) -> void {
        o.asset_ = nullptr;
        o.asset_ptr_.asset_id = static_cast<AssetId>(v["asset_id"].get<serde::Value::Integer>());
    }

private:
    mutable Ptr<Asset> asset_;
    AssetPtr asset_ptr_;
};

}
