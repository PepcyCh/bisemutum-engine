#pragma once

#include "asset.hpp"
#include "scene_object.hpp"

namespace bi::rt {

struct Prefab final {
    static constexpr std::string_view asset_type_name = "Prefab";

    static auto load(Dyn<rt::IFile>::Ref file) -> rt::AssetAny;

    auto instantiate() -> void;

private:
    Ptr<SceneObject> object_;
};

}
