#include "resource.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFXRG_NAMESPACE_BEGIN

BufferBuilder &BufferBuilder::Size(uint64_t size) {
    desc_.size = size;
    return *this;
}

BufferBuilder &BufferBuilder::Usage(BitFlags<gfx::BufferUsage> usage) {
    desc_.usages = usage;
    return *this;
}

BufferBuilder &BufferBuilder::MemUpload() {
    desc_.memory_property = gfx::BufferMemoryProperty::eCpuToGpu;
    return *this;
}

BufferBuilder &BufferBuilder::MemReadback() {
    desc_.memory_property = gfx::BufferMemoryProperty::eGpuToCpu;
    return *this;
}

TextureBuilder &TextureBuilder::Dim1D(gfx::ResourceFormat format, uint32_t width, uint32_t arrays) {
    desc_.format = format;
    desc_.dim = gfx::TextureDimension::e1D;
    desc_.extent = gfx::Extent3D {
        .width = width,
        .height = 1,
        .depth_or_layers = arrays,
    };
    return *this;
}

TextureBuilder &TextureBuilder::Dim2D(gfx::ResourceFormat format, uint32_t width, uint32_t height, uint32_t arrays) {
    desc_.format = format;
    desc_.dim = gfx::TextureDimension::e2D;
    desc_.extent = gfx::Extent3D {
        .width = width,
        .height = height,
        .depth_or_layers = arrays,
    };
    return *this;
}

TextureBuilder &TextureBuilder::DimCube(gfx::ResourceFormat format, uint32_t width, uint32_t height, uint32_t arrays) {
    desc_.format = format;
    desc_.dim = gfx::TextureDimension::e2D;
    desc_.extent = gfx::Extent3D {
        .width = width,
        .height = height,
        .depth_or_layers = 6 * arrays,
    };
    return *this;
}

TextureBuilder &TextureBuilder::Dim3D(gfx::ResourceFormat format, uint32_t width, uint32_t height, uint32_t depth) {
    desc_.format = format;
    desc_.dim = gfx::TextureDimension::e3D;
    desc_.extent = gfx::Extent3D {
        .width = width,
        .height = height,
        .depth_or_layers = depth,
    };
    return *this;
}

TextureBuilder &TextureBuilder::Mipmap(uint32_t levels) {
    uint32_t max_size = std::max(desc_.extent.width, desc_.extent.height);
    if (desc_.dim == gfx::TextureDimension::e3D) {
        max_size = std::max(max_size, desc_.extent.depth_or_layers);
    }
    desc_.levels = std::min<uint32_t>(levels, 1 + std::log2(max_size));
    return *this;
}

TextureBuilder &TextureBuilder::Usage(BitFlags<gfx::TextureUsage> usage) {
    desc_.usages = usage;
    return *this;
}

BISMUTH_GFXRG_NAMESPACE_END

BISMUTH_NAMESPACE_END
