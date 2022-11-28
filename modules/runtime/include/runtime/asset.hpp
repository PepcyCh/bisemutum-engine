#pragma once

#include <filesystem>

#include <core/container.hpp>
#include <core/ptr.hpp>

#include "mod.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_RT_NAMESPACE_BEGIN

class Asset {
public:
    Asset(const std::filesystem::path &abs_path) : abs_path_(abs_path) {}

    virtual ~Asset() = default;

    virtual void Serialize() const = 0;

    virtual bool IsLoaded() const = 0;
    virtual void Load() = 0;

    const std::filesystem::path &GetAbsolutePath() const { return abs_path_; }
    std::filesystem::path GetRelativePath() const;

protected:
    std::filesystem::path abs_path_;
};

using AssetConstructor = Ptr<Asset>(const std::filesystem::path &);

template <typename T>
concept IsAsset = std::derived_from<T, Asset>
    && std::convertible_to<decltype(T::kTypeName), const char *>
    && std::same_as<decltype(T::Construct), AssetConstructor>;

class AssetManager final {
public:
    ~AssetManager();

    template <IsAsset T>
    void Register() {
        constructors_.insert({ T::kTypeName, &T::Construct });
    }

    Ref<Asset> Construct(const char *asset_type, const std::filesystem::path &path);

    const std::filesystem::path &GetBasePath() const { return base_path_; }

private:
    friend class World;

    std::filesystem::path base_path_;
    HashMap<const char *, AssetConstructor *> constructors_;
    HashMap<std::filesystem::path, Ptr<Asset>> constructed_assets_;
};

BISMUTH_RT_NAMESPACE_END

BISMUTH_NAMESPACE_END
