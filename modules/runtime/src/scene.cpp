#include <runtime/scene.hpp>

#include <fstream>

#include <core/module_manager.hpp>
#include <nlohmann/json.hpp>

BISMUTH_NAMESPACE_BEGIN

BISMUTH_RT_NAMESPACE_BEGIN

Ref<SceneObject> Scene::CreateObject(std::string_view name) {
    auto object = Ptr<SceneObject>::UnsafeMake(new SceneObject(RefThis(), name, registry_));
    auto ref_object = object.AsRef();
    objects_.emplace_back(std::move(object));
    return ref_object;
}

void SceneAsset::Serialize() const {
    auto scene_json = nlohmann::json {
        { "objects", {} },
    };

    for (const auto &object : scene_->objects_) {
        scene_json.push_back(object->Serialize());
    }

    auto scene_json_str = scene_json.dump(2);
    std::ofstream fout(abs_path_);
    fout.write(scene_json_str.c_str(), scene_json_str.size());
}

bool SceneAsset::IsLoaded() const {
    return scene_.IsInitialized();
}

void SceneAsset::Load() {
    scene_ = Ptr<Scene>::Make();
    if (!std::filesystem::exists(abs_path_)) {
        return;
    }

    try {
        auto scene_json = nlohmann::json::parse(std::ifstream(abs_path_));

        if (auto objects_it = scene_json.find("objects"); objects_it != scene_json.end()) {
            for (const auto &object_json : *objects_it) {
                const auto &name = object_json["name"].get<std::string>();
                auto object = scene_->CreateObject(name);
                object->Deserialize(object_json);
            }
        }
    } catch (const std::exception &e) {
        BI_WARN(ModuleManager::Get<RuntimeModule>()->Lgr(),
            "failed to load scene at {}, exception: {}", abs_path_.string(), e.what());
    }
}

Ptr<Asset> SceneAsset::Construct(const std::filesystem::path &path) {
    return Ptr<SceneAsset>::Make(path);
}

BISMUTH_RT_NAMESPACE_END

BISMUTH_NAMESPACE_END
