#pragma once

#include "scene.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_RT_NAMESPACE_BEGIN

class World final {
public:
    World(const std::filesystem::path &path);

    void Serialize() const;

    static std::optional<Ptr<World>> Deserialize(const std::filesystem::path &path);

    Ref<Scene> GetCurrentScene() const { return scenes_[curr_scene_]->GetScene(); }

    Ref<SceneAsset> CreateScene(const std::filesystem::path &path);

private:
    void CreateDefaultScene();

    std::filesystem::path path_;
    Vec<Ref<SceneAsset>> scenes_;
    size_t curr_scene_ = 0;
    size_t default_scene_ = 0;
};

BISMUTH_RT_NAMESPACE_END

BISMUTH_NAMESPACE_END
