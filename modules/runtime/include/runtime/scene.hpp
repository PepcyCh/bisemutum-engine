#pragma once

#include "object.hpp"
#include "asset.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_RT_NAMESPACE_BEGIN

class Scene final : public RefFromThis<Scene> {
public:
    Ref<SceneObject> CreateObject(std::string_view name);

private:
    friend SceneObject;
    friend class SceneAsset;

    entt::registry registry_;
    Vec<Ptr<SceneObject>> objects_;
};

class SceneAsset final : public Asset {
public:
    SceneAsset(const std::filesystem::path &abs_path) : Asset(abs_path) {}

    static constexpr const char *kTypeName = "runtime::SceneAsset";

    void Serialize() const override;

    bool IsLoaded() const override;
    void Load() override;

    static Ptr<Asset> Construct(const std::filesystem::path &path);

    Ref<Scene> GetScene() const { return scene_.AsRef(); }

private:
    Ptr<Scene> scene_;
};

BISMUTH_RT_NAMESPACE_END

BISMUTH_NAMESPACE_END
