#include <bisemutum/runtime/asset_manager.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/vfs.hpp>
#include <bisemutum/containers/hash.hpp>

namespace bi::rt {

struct AssetManager::Impl final {
    auto register_asset(std::string_view type, std::function<AssetLoader>&& loader) -> void {
        loaders.insert({type, std::move(loader)});
    }

    auto state_of(std::string_view asset_path) -> AssetState {
        auto it = assets.find(asset_path);
        if (it == assets.end()) {
            it = assets.insert({std::string{asset_path}, Asset{}}).first;
            if (!g_engine->file_system()->has_file(asset_path)) {
                it->second.state = AssetState::not_found;
            }
        }
        return it->second.state;
    }

    auto load_asset(std::string_view type, std::string_view asset_path) -> AssetAny* {
        auto it = assets.find(asset_path);
        if (it == assets.end()) {
            it = assets.insert({std::string{asset_path}, Asset{}}).first;
            if (!g_engine->file_system()->has_file(asset_path)) {
                it->second.state = AssetState::not_found;
            }
        }
        if (it->second.state == AssetState::not_loaded) {
            auto& loader = loaders.at(type);
            auto asset_file = g_engine->file_system()->get_file(asset_path).value();
            it->second.content = loader(*&asset_file);
            it->second.state = it->second.content.has_value() ? AssetState::loaded : AssetState::error;
            return std::addressof(it->second.content);
        } else {
            return std::addressof(it->second.content);
        }
    }

    struct Asset final {
        AssetAny content;
        AssetState state = AssetState::not_loaded;
    };
    std::unordered_map<std::string_view, std::function<AssetLoader>> loaders;
    StringHashMap<Asset> assets;
};

AssetManager::AssetManager() = default;
AssetManager::~AssetManager() = default;

auto AssetManager::register_asset(std::string_view type, std::function<AssetLoader> loader) -> void {
    impl()->register_asset(type, std::move(loader));
}

auto AssetManager::state_of(std::string_view asset_path) -> AssetState {
    return impl()->state_of(asset_path);
}

auto AssetManager::load_asset(std::string_view type, std::string_view asset_path) -> AssetAny* {
    return impl()->load_asset(type, asset_path);
}

}
