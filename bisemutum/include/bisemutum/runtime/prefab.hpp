#pragma once

#include "asset.hpp"
#include "scene_object.hpp"

namespace bi::rt {

struct Prefab final {
    static constexpr std::string_view asset_type_name = "Prefab";

    static auto load(Dyn<rt::IFile>::Ref file) -> rt::AssetAny;

    static auto create_from(CRef<SceneObject> object, std::string_view dst_path) -> void;

    auto save(Dyn<rt::IFile>::Ref file) const -> void;

    auto instantiate() -> void;

private:
    Ptr<SceneObject> object_;
};

}
