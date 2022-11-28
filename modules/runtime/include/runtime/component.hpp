#pragma once

#include <concepts>

#include <core/container.hpp>
#include <nlohmann/json_fwd.hpp>

#include "mod.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_RT_NAMESPACE_BEGIN

class SceneObject;

class Component {
public:
    virtual ~Component() = default;

    virtual void Init() {}

    virtual nlohmann::json Serialize() const = 0;

protected:
    friend SceneObject;

    Ref<SceneObject> object_;
};

using ComponentDeserializer = void(SceneObject &object, const nlohmann::json &value);

template <typename T>
concept IsComponent = std::derived_from<T, Component>
    && std::convertible_to<decltype(T::kTypeName), const char *>
    && std::same_as<decltype(T::Deserialize), ComponentDeserializer>;

class ComponentManager final {
public:
    ~ComponentManager();

    template <IsComponent T>
    void Register() {
        deserializers_.insert({ T::kTypeName, &T::Deserialize });
    }

    void Deserialize(SceneObject &object, const nlohmann::json &value);

private:
    HashMap<const char *, ComponentDeserializer *> deserializers_;
};

BISMUTH_RT_NAMESPACE_END

BISMUTH_NAMESPACE_END
