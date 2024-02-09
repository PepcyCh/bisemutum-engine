#pragma once

#include "material.hpp"
#include "../runtime/asset.hpp"

namespace bi {

struct MeshRendererComponent final {
    static constexpr std::string_view component_type_name = "MeshRendererComponent";

    std::vector<rt::TAssetPtr<MaterialAsset>> materials;
    uint32_t submesh_start_index = 0;
};
BI_SREFL(type(MeshRendererComponent), field(materials), field(submesh_start_index))

}
