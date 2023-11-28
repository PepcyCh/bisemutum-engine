#include "register.hpp"

#include <bisemutum/runtime/asset_manager.hpp>

#include <bisemutum/runtime/prefab.hpp>
#include <bisemutum/scene_basic/static_mesh.hpp>
#include <bisemutum/scene_basic/texture.hpp>
#include <bisemutum/scene_basic/material.hpp>

namespace bi {

auto register_assets(Ref<rt::AssetManager> mgr) -> void {
    mgr->register_asset<rt::Prefab>();

    mgr->register_asset<MaterialAsset>();
    mgr->register_asset<TextureAsset>();
    mgr->register_asset<StaticMesh>();
}

}
