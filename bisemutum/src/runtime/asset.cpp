#include <bisemutum/runtime/asset.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/asset_manager.hpp>

namespace bi::rt {

auto AssetPtr::state() const -> AssetState {
    return g_engine->asset_manager()->state_of(asset_path);
}

auto AssetPtr::load(std::string_view type) const -> std::any* {
    return g_engine->asset_manager()->load_asset(type, asset_path);
}

}
