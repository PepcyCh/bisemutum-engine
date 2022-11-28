#include <runtime/asset.hpp>

#include <core/module_manager.hpp>

BISMUTH_NAMESPACE_BEGIN

BISMUTH_RT_NAMESPACE_BEGIN

std::filesystem::path Asset::GetRelativePath() const {
    return std::filesystem::relative(abs_path_, ModuleManager::Get<RuntimeModule>()->AssetMgr()->GetBasePath());
}

AssetManager::~AssetManager() = default;

Ref<Asset> AssetManager::Construct(const char *asset_type, const std::filesystem::path &path) {
    auto it = constructed_assets_.find(path);
    if (it == constructed_assets_.end()) {
        auto constructor = constructors_.find(asset_type);
        if (constructor == constructors_.end()) {
            BI_CRTICAL(ModuleManager::Get<RuntimeModule>()->Lgr(),
                "try to construct an asset with unregistered type '{}'", asset_type);
        } else {
            it = constructed_assets_.insert({ path, constructor->second(base_path_ / path) }).first;
        }
    }
    return it->second.AsRef();
}

BISMUTH_RT_NAMESPACE_END

BISMUTH_NAMESPACE_END
