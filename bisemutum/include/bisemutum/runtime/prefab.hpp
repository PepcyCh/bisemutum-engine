#pragma once

#include "asset.hpp"
#include "component_manager.hpp"

namespace bi::rt {

struct Prefab final {
    static constexpr std::string_view asset_type_name = "Prefab";

    static auto load(Dyn<rt::IFile>::Ref file) -> rt::AssetAny;

private:
    std::string name_;
    std::vector<std::function<ComponentDeserializer>> components_;
};

}
