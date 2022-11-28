#include <runtime/world.hpp>

#include <fstream>

#include <core/module_manager.hpp>
#include <nlohmann/json.hpp>

BISMUTH_NAMESPACE_BEGIN

BISMUTH_RT_NAMESPACE_BEGIN

World::World(const std::filesystem::path &path) : path_(path) {}

void World::Serialize() const {
    auto world_json = nlohmann::json {
        { "default_scene", default_scene_ },
        { "scenes", {} },
    };
    auto &scenes_json = world_json.at("scenes");
    for (const auto &scene : scenes_) {
        scenes_json.push_back(nlohmann::json {
            { "type", SceneAsset::kTypeName },
            { "path", scene->GetRelativePath(), },
        });
    }

    auto world_json_str = world_json.dump(2);
    std::ofstream fout(path_);
    fout.write(world_json_str.c_str(), world_json_str.size());
}

std::optional<Ptr<World>> World::Deserialize(const std::filesystem::path &path) {
    try {
        auto world_json = nlohmann::json::parse(std::ifstream(path));

        auto world = Ptr<World>::Make(path);
        ModuleManager::Get<RuntimeModule>()->AssetMgr()->base_path_ = path.parent_path();
        if (auto scenes_it = world_json.find("scenes"); scenes_it != world_json.end()) {
            world->scenes_.reserve(scenes_it->size());
            for (const auto &scene_json : *scenes_it) {
                const auto &asset_type = scene_json["type"].get<std::string>();
                const auto &asset_path = scene_json["path"].get<std::string>();
                auto scene = ModuleManager::Get<RuntimeModule>()->AssetMgr()->Construct(
                    asset_type.c_str(), asset_path).CastTo<SceneAsset>();
                world->scenes_.push_back(scene);
            }
        }
        if (auto default_scene_it = world_json.find("default_scene"); default_scene_it != world_json.end()) {
            world->default_scene_ = default_scene_it->get<size_t>();
        } else {
            world->default_scene_ = 0;
        }
        if (world->scenes_.empty()) {
            world->CreateDefaultScene();
        }
        world->curr_scene_ = world->default_scene_;
        return world;
    } catch (const std::exception &e) {
        BI_WARN(ModuleManager::Get<RuntimeModule>()->Lgr(),
            "failed to load world at {}, exception: {}", path.string(), e.what());
        return std::nullopt;
    }
}

Ref<SceneAsset> World::CreateScene(const std::filesystem::path &path) {
    auto scene = ModuleManager::Get<RuntimeModule>()->AssetMgr()->Construct(
        SceneAsset::kTypeName, path).CastTo<SceneAsset>();
    scene->Load();
    scenes_.push_back(scene);
    return scene;
}

void World::CreateDefaultScene() {
    CreateScene("scene.json");
    default_scene_ = 0;
}

BISMUTH_RT_NAMESPACE_END

BISMUTH_NAMESPACE_END
