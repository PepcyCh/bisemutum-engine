#include <bisemutum/rhi/resource.hpp>

#include <bisemutum/prelude/misc.hpp>

namespace bi::rhi {

auto Texture::get_automatic_view_type() const -> TextureViewType {
    switch (desc_.dim) {
        case TextureDimension::d1:
            return desc_.extent.depth_or_layers == 1 ? TextureViewType::d1 : TextureViewType::d1_array;
        case TextureDimension::d2:
            return desc_.extent.depth_or_layers == 1 ? TextureViewType::d2 : TextureViewType::d2_array;
        case TextureDimension::d3:
            return TextureViewType::d3;
        default: unreachable();
    }
}

}
