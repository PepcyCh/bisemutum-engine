#pragma once

#include <core/module.hpp>
#include <core/ptr.hpp>

BISMUTH_NAMESPACE_BEGIN

#define BISMUTH_RT_NAMESPACE_BEGIN namespace rt {
#define BISMUTH_RT_NAMESPACE_END }

BISMUTH_RT_NAMESPACE_BEGIN

class AssetManager;
class ComponentManager;

class RuntimeModule final : public Module {
public:
    RuntimeModule();
    ~RuntimeModule();

    static constexpr const char *kName = "runtime";

    Logger Lgr() const override { return logger_; }

    Ref<AssetManager> AssetMgr() const { return asset_manager_.AsRef(); }

    Ref<ComponentManager> ComponentMgr() const { return component_manager_.AsRef(); }

private:
    Logger logger_;
    Ptr<AssetManager> asset_manager_;
    Ptr<ComponentManager> component_manager_;
};

BISMUTH_RT_NAMESPACE_END

BISMUTH_NAMESPACE_END
