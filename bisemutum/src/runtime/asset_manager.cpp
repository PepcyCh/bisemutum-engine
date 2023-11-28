#include <bisemutum/runtime/asset_manager.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/vfs.hpp>
#include <bisemutum/runtime/logger.hpp>
#include <bisemutum/containers/hash.hpp>

namespace bi::rt {

struct AssetMetadata final {
    uint64_t id;
    std::string type;
    std::string path;
};
BI_SREFL(type(AssetMetadata), field(id), field(type), field(path));

struct AssetManager::Impl final {
    auto initialize(Dyn<IFile>::Ref metadata_file) -> bool {
        auto metadata_str = metadata_file.read_string_data();

        std::vector<AssetMetadata> metadata;
        try {
            auto value = serde::Value::from_toml(metadata_str);
            metadata = value["assets"].get<decltype(metadata)>();
        } catch (std::exception const& e) {
            log::critical("general", "Asset metadata file is invalid: {}", e.what());
            return false;
        }

        next_id = 0;
        assets.reserve(metadata.size());
        for (auto &data : metadata) {
            auto it = assets.try_emplace(data.id, Asset{.metadata = std::move(data)}).first;
            next_id = std::max(next_id, data.id);
            assets_path_map[it->second.metadata.path] = static_cast<AssetId>(it->first);
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

    auto asset_id_of(std::string_view path) -> AssetId {
        if (auto it = assets_path_map.find(path); it != assets_path_map.end()) {
            return it->second;
        } else {
            return AssetId::invalid;
        }
    }

    auto load_asset(std::string_view type, AssetId asset_id) -> AssetAny* {
        auto it = assets.find(static_cast<uint64_t>(asset_id));
        if (it == assets.end()) {
            return nullptr;
        }
        if (it->second.state == AssetState::not_loaded) {
            auto& loader = asset_functions.at(type).loader;
            auto asset_file = g_engine->file_system()->get_file(it->second.metadata.path).value();
            it->second.content = loader(*&asset_file);
            it->second.state = it->second.content.has_value() ? AssetState::loaded : AssetState::error;
            return std::addressof(it->second.content);
        } else {
            return std::addressof(it->second.content);
        }
    }

    auto create_asset(std::string_view asset_path, AssetAny&& asset) -> std::pair<AssetId, AssetAny*> {
        auto id = static_cast<AssetId>(next_id);
        auto it = assets.try_emplace(next_id++).first;
        it->second.content = std::move(asset);
        it->second.state = AssetState::loaded;
        it->second.dirty = true;
        assets_path_map[it->second.metadata.path] = id;
        return {id, std::addressof(it->second.content)};
    }

    auto save_all_assets() -> void {
        for (auto& [path, asset] : assets) {
            if (!asset.dirty) { continue; }
        }
    }

    struct Asset final {
        AssetMetadata metadata;
        AssetAny content;
        AssetState state = AssetState::not_loaded;
        bool dirty = false;
    };
    std::unordered_map<uint64_t, Asset> assets;
    std::unordered_map<std::string_view, AssetId> assets_path_map;
    uint64_t next_id;
    std::unordered_map<std::string_view, AssetFunctions> asset_functions;
};

AssetManager::AssetManager() = default;
AssetManager::~AssetManager() = default;

auto AssetManager::initialize(Dyn<IFile>::Ref metadata_file) -> bool {
    return impl()->initialize(metadata_file);
}

auto AssetManager::register_asset(std::string_view type, AssetFunctions&& functions) -> void {
    impl()->register_asset(type, std::move(functions));
}

auto AssetManager::state_of(AssetId asset_id) -> AssetState {
    return impl()->state_of(asset_id);
}

auto AssetManager::asset_id_of(std::string_view path) -> AssetId {
    return impl()->asset_id_of(path);
}

auto AssetManager::load_asset(std::string_view type, AssetId asset_id) -> AssetAny* {
    return impl()->load_asset(type, asset_id);
}

auto AssetManager::create_asset(std::string_view asset_path, AssetAny asset) -> std::pair<AssetId, AssetAny*> {
    return impl()->create_asset(asset_path, std::move(asset));
}

auto AssetManager::save_all_assets() -> void {
    impl()->save_all_assets();
}

}
