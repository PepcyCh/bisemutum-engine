#include <bisemutum/runtime/asset_manager.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/vfs.hpp>
#include <bisemutum/runtime/logger.hpp>
#include <bisemutum/containers/hash.hpp>

namespace bi::rt {

namespace {

auto is_asset_id_builtin(uint64_t asset_id) -> bool {
    return (asset_id & (1ull << 62)) != 0;
}

}

struct AssetManager::Impl final {
    auto initialize(Dyn<IFile>::Ref metadata_file) -> bool {
        next_id = 0;

        auto engine_assets_metadata_file =
            g_engine->file_system()->get_file("/bisemutum/assets/asset_metadata.toml").value();
        if (!load_metadata_from(*&engine_assets_metadata_file)) { return false; }

        if (!load_metadata_from(metadata_file)) { return false; }

        return true;
    }
    auto load_metadata_from(Dyn<IFile>::Ref metadata_file) -> bool {
        auto metadata_str = metadata_file.read_string_data();

        std::vector<AssetMetadata> metadata;
        try {
            auto value = serde::Value::from_toml(metadata_str);
            if (value.contains("assets")) {
                metadata = value["assets"].get<decltype(metadata)>();
            }
        } catch (std::exception const& e) {
            log::critical("general", "Asset metadata file is invalid: {}", e.what());
            return false;
        }

        assets.reserve(assets.size() + metadata.size());
        for (auto &data : metadata) {
            auto it = assets.try_emplace(data.id, Asset{.metadata = std::move(data)}).first;
            if (!is_asset_id_builtin(data.id)) {
                next_id = std::max(next_id, data.id);
            }
            auto asset_id = static_cast<AssetId>(it->first);
            assets_path_map[it->second.metadata.path] = asset_id;
            assets_type_map[it->second.metadata.type].push_back(asset_id);
        }
        ++next_id;

        return true;
    }

    auto register_asset(std::string_view type, AssetFunctions&& functions) -> void {
        asset_functions.insert({type, std::move(functions)});
    }

    auto state_of(AssetId asset_id) -> AssetState {
        auto it = assets.find(static_cast<uint64_t>(asset_id));
        if (it == assets.end()) {
            return AssetState::not_found;
        }
        return it->second.state;
    }

    auto metadata_of(AssetId asset_id) const -> CPtr<AssetMetadata> {
        auto it = assets.find(static_cast<uint64_t>(asset_id));
        if (it == assets.end()) {
            return nullptr;
        }
        return &it->second.metadata;
    }

    auto all_metadata_of_type(std::string_view type) const -> std::vector<CRef<AssetMetadata>> {
        if (auto it = assets_type_map.find(type); it != assets_type_map.end()) {
            std::vector<CRef<AssetMetadata>> ret;
            ret.reserve(it->second.size());
            for (auto asset_id : it->second) {
                ret.push_back(assets.at(static_cast<uint64_t>(asset_id)).metadata);
            }
            return ret;
        } else {
            return {};
        }
    }

    auto asset_id_of(std::string_view path) -> AssetId {
        if (auto it = assets_path_map.find(path); it != assets_path_map.end()) {
            return it->second;
        } else {
            return AssetId::invalid;
        }
    }

    auto load_asset(AssetId asset_id) -> AssetAny* {
        auto it = assets.find(static_cast<uint64_t>(asset_id));
        if (it == assets.end()) {
            return nullptr;
        }
        if (it->second.state == AssetState::not_loaded) {
            auto& loader = asset_functions.at(it->second.metadata.type).loader;
            auto asset_file = g_engine->file_system()->get_file(it->second.metadata.path).value();
            it->second.content = loader(*&asset_file);
            it->second.state = it->second.content.has_value() ? AssetState::loaded : AssetState::error;
            return std::addressof(it->second.content);
        } else {
            return std::addressof(it->second.content);
        }
    }

    auto create_asset(
        std::string_view asset_type_name, std::string_view asset_path, AssetAny&& asset
    ) -> std::pair<AssetId, AssetAny*> {
        auto id = static_cast<AssetId>(next_id);
        auto it = assets.try_emplace(next_id++).first;
        it->second.content = std::move(asset);
        it->second.state = AssetState::loaded;
        it->second.dirty = true;
        it->second.metadata = {it->first, std::string{asset_type_name}, std::string{asset_path}};
        assets_path_map[it->second.metadata.path] = id;
        assets_type_map[it->second.metadata.type].push_back(id);
        return {id, std::addressof(it->second.content)};
    }

    auto save_all_assets(Dyn<IFile>::Ref metadata_file, bool force) -> void {
        for (auto& [_, asset] : assets) {
            if ((!force && !asset.dirty) || !asset.content.has_value()) { continue; }
            auto asset_file = g_engine->file_system()->create_file(asset.metadata.path).value();
            auto& saver = asset_functions[asset.metadata.type].saver;
            saver(*&asset_file, asset.content);
            asset.dirty = false;
        }

        std::vector<AssetMetadata> metadata{};
        metadata.reserve(assets.size());
        for (auto& [_, asset] : assets) {
            if (is_asset_id_builtin(asset.metadata.id)) { continue; }
            metadata.push_back(asset.metadata);
        }
        serde::Value value{};
        serde::to_value(value["assets"], metadata);
        auto metadata_toml = value.to_toml();
        metadata_file.write_string_data(metadata_toml);
    }

    struct Asset final {
        AssetMetadata metadata;
        AssetAny content;
        AssetState state = AssetState::not_loaded;
        bool dirty = false;
    };
    std::unordered_map<uint64_t, Asset> assets;
    std::unordered_map<std::string_view, AssetId> assets_path_map;
    std::unordered_map<std::string_view, std::vector<AssetId>> assets_type_map;
    uint64_t next_id;
    std::unordered_map<std::string_view, AssetFunctions> asset_functions;
};

AssetManager::AssetManager() = default;

auto AssetManager::initialize(Dyn<IFile>::Ref metadata_file) -> bool {
    return impl()->initialize(metadata_file);
}

auto AssetManager::register_asset(std::string_view type, AssetFunctions&& functions) -> void {
    impl()->register_asset(type, std::move(functions));
}

auto AssetManager::state_of(AssetId asset_id) -> AssetState {
    return impl()->state_of(asset_id);
}

auto AssetManager::metadata_of(AssetId asset_id) const -> CPtr<AssetMetadata> {
    return impl()->metadata_of(asset_id);
}

auto AssetManager::all_metadata_of_type(std::string_view type) const -> std::vector<CRef<AssetMetadata>> {
    return impl()->all_metadata_of_type(type);
}

auto AssetManager::asset_id_of(std::string_view path) -> AssetId {
    return impl()->asset_id_of(path);
}

auto AssetManager::load_asset(AssetId asset_id) -> AssetAny* {
    return impl()->load_asset(asset_id);
}

auto AssetManager::create_asset(
    std::string_view asset_type_name, std::string_view asset_path, AssetAny asset
) -> std::pair<AssetId, AssetAny*> {
    return impl()->create_asset(asset_type_name, asset_path, std::move(asset));
}

auto AssetManager::save_all_assets(Dyn<IFile>::Ref metadata_file, bool force) -> void {
    impl()->save_all_assets(metadata_file, force);
}

}
