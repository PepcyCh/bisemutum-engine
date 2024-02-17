#include <bisemutum/runtime/asset.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/asset_manager.hpp>
#include <bisemutum/runtime/logger.hpp>
#include <bisemutum/editor/basic.hpp>

namespace bi::rt {

auto AssetPtr::from_path(std::string_view asset_path) -> AssetPtr {
    return AssetPtr{
        .asset_id = g_engine->asset_manager()->asset_id_of(asset_path),
    };
}

auto AssetPtr::state() const -> AssetState {
    return g_engine->asset_manager()->state_of(asset_id);
}

auto AssetPtr::load() const -> AssetAny* {
    return g_engine->asset_manager()->load_asset(asset_id);
}

auto AssetPtr::edit(std::string_view type) -> bool {
    auto asset_mgr = g_engine->asset_manager();

    auto curr_asset = asset_mgr->metadata_of(asset_id);
    auto all_assets = asset_mgr->all_metadata_of_type(type);
    auto edited = false;
    auto label = fmt::format("{}##{}", type, reinterpret_cast<size_t>(this));
    if (ImGui::BeginCombo(label.c_str(), curr_asset ? curr_asset->path.c_str() : "<null>")) {
        for (auto& asset : all_assets) {
            auto selected = asset->id == static_cast<uint64_t>(asset_id);
            if (ImGui::Selectable(asset->path.c_str(), selected) && !selected) {
                edited = true;
                asset_id = static_cast<AssetId>(asset->id);
            }
        }
        ImGui::EndCombo();
    }
    return edited;
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
