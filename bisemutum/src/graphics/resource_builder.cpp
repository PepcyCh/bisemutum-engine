#include <bisemutum/graphics/resource_builder.hpp>

#include <cmath>

namespace bi::gfx {

auto BufferBuilder::size(uint64_t size) -> BufferBuilder& {
    desc_.size = size;
    return *this;
}
auto BufferBuilder::usage(BitFlags<rhi::BufferUsage> usage) -> BufferBuilder& {
    desc_.usages = usage;
    return *this;
}
auto BufferBuilder::mem_upload() -> BufferBuilder& {
    desc_.memory_property = rhi::BufferMemoryProperty::cpu_to_gpu;
    return *this;
}
auto BufferBuilder::mem_readback() -> BufferBuilder& {
    desc_.memory_property = rhi::BufferMemoryProperty::gpu_to_cpu;
    return *this;
}

auto TextureBuilder::dim_1d(rhi::ResourceFormat format, uint32_t width, uint32_t arrays) -> TextureBuilder& {
    desc_.format = format;
    desc_.dim = rhi::TextureDimension::d1;
    desc_.extent = rhi::Extent3D {
        .width = width,
        .height = 1,
        .depth_or_layers = arrays,
    };
    return *this;
}
auto TextureBuilder::dim_2d(
    rhi::ResourceFormat format, uint32_t width, uint32_t height, uint32_t arrays
) -> TextureBuilder& {
    desc_.format = format;
    desc_.dim = rhi::TextureDimension::d2;
    desc_.extent = rhi::Extent3D {
        .width = width,
        .height = height,
        .depth_or_layers = arrays,
    };
    return *this;
}
auto TextureBuilder::dim_cube(rhi::ResourceFormat format, uint32_t width, uint32_t arrays) -> TextureBuilder& {
    desc_.format = format;
    desc_.dim = rhi::TextureDimension::d2;
    desc_.extent = rhi::Extent3D{
        .width = width,
        .height = width,
        .depth_or_layers = 6 * arrays,
    };
    return *this;
}
auto TextureBuilder::dim_3d(
    rhi::ResourceFormat format, uint32_t width, uint32_t height, uint32_t depth
) -> TextureBuilder& {
    desc_.format = format;
    desc_.dim = rhi::TextureDimension::d3;
    desc_.extent = rhi::Extent3D{
        .width = width,
        .height = height,
        .depth_or_layers = depth,
    };
    return *this;
}
auto TextureBuilder::mipmap(uint32_t levels) -> TextureBuilder& {
    uint32_t max_size = std::max(desc_.extent.width, desc_.extent.height);
    if (desc_.dim == rhi::TextureDimension::d3) {
        max_size = std::max(max_size, desc_.extent.depth_or_layers);
    }
    desc_.levels = std::min<uint32_t>(levels, 1 + std::log2(max_size));
    return *this;
}
auto TextureBuilder::usage(BitFlags<rhi::TextureUsage> usage) -> TextureBuilder& {
    desc_.usages = usage;
    return *this;
}

}
