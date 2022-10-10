#pragma once

#include "graphics/resource.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class BufferBuilder {
public:
    operator gfx::BufferDesc() const { return desc_; }

    BufferBuilder &Size(uint64_t size);

    BufferBuilder &Usage(BitFlags<gfx::BufferUsage> usage);

    BufferBuilder &MemUpload();
    BufferBuilder &MemReadback();

private:
    gfx::BufferDesc desc_;
};

class TextureBuilder {
public:
    operator gfx::TextureDesc() const { return desc_; }

    TextureBuilder &Dim1D(gfx::ResourceFormat format, uint32_t width, uint32_t arrays = 1);
    TextureBuilder &Dim2D(gfx::ResourceFormat format, uint32_t width, uint32_t height, uint32_t arrays = 1);
    TextureBuilder &DimCube(gfx::ResourceFormat format, uint32_t width, uint32_t height, uint32_t arrays = 1);
    TextureBuilder &Dim3D(gfx::ResourceFormat format, uint32_t width, uint32_t height, uint32_t depth);

    TextureBuilder &Mipmap(uint32_t levels = ~0u);

    TextureBuilder &Usage(BitFlags<gfx::TextureUsage> usage);

private:
    gfx::TextureDesc desc_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
