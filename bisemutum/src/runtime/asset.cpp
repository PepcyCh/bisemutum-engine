#include <bisemutum/runtime/asset.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/asset_manager.hpp>
#include <bisemutum/runtime/logger.hpp>

namespace bi::rt {

auto AssetPtr::state() const -> AssetState {
    return g_engine->asset_manager()->state_of(asset_id);
}

auto AssetPtr::load() const -> AssetAny* {
    return g_engine->asset_manager()->load_asset(asset_id);
}

auto check_if_binary_asset_valid(
    std::string_view filename, uint32_t magic_number, std::string const& type_name, std::string_view expected_type_name
) -> bool {
    if (magic_number != rt::asset_magic_number) {
        log::critical(
            "general",
            "Failed to load {} ('{}'): Invalid file data.", expected_type_name, filename
        );
        return false;
    }
    if (type_name != expected_type_name) {
        log::critical(
            "general",
            "Failed to load {} ('{}'): Incorrect asset type name.", expected_type_name, filename
        );
        return false;
    }
    return true;
}

}
