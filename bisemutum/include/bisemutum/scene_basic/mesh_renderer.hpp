#pragma once

#include "material.hpp"
#include "../runtime/asset.hpp"

namespace bi {

struct MeshRendererComponent final {
    static constexpr std::string_view component_type_name = "MeshRendererComponent";

    rt::TAssetPtr<MaterialAsset> material;
};
BI_SREFL(type(MeshRendererComponent), field(material))

}
