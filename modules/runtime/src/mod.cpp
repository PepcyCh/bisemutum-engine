#include <runtime/mod.hpp>

#include <core/logger.hpp>
#include <runtime/scene.hpp>

BISMUTH_NAMESPACE_BEGIN

BISMUTH_RT_NAMESPACE_BEGIN

RuntimeModule::~RuntimeModule() = default;

RuntimeModule::RuntimeModule() {
    logger_ = LoggerManager::Get().RegisterLogger("Runtime");
    asset_manager_ = Ptr<AssetManager>::Make();
    component_manager_ = Ptr<ComponentManager>::Make();

    asset_manager_->Register<SceneAsset>();
}

BISMUTH_RT_NAMESPACE_END

BISMUTH_NAMESPACE_END
