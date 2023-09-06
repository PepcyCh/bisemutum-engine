#pragma once

#include "../rhi/resource.hpp"

namespace bi::gfx {

struct BufferBuilder final {
    operator rhi::BufferDesc() const { return desc_; }

    auto size(uint64_t size) -> BufferBuilder&;
    auto usage(BitFlags<rhi::BufferUsage> usage) -> BufferBuilder&;

    auto mem_upload() -> BufferBuilder&;
    auto mem_readback() -> BufferBuilder&;

private:
    rhi::BufferDesc desc_;
};

struct TextureBuilder final {
    operator rhi::TextureDesc() const { return desc_; }

    auto dim_1d(rhi::ResourceFormat format, uint32_t width, uint32_t arrays = 1) -> TextureBuilder&;
    auto dim_2d(rhi::ResourceFormat format, uint32_t width, uint32_t height, uint32_t arrays = 1) -> TextureBuilder&;
    auto dim_cube(rhi::ResourceFormat format, uint32_t width, uint32_t arrays = 1) -> TextureBuilder&;
    auto dim_3d(rhi::ResourceFormat format, uint32_t width, uint32_t height, uint32_t depth) -> TextureBuilder&;

    auto mipmap(uint32_t levels = ~0u) -> TextureBuilder&;

    auto usage(BitFlags<rhi::TextureUsage> usage) -> TextureBuilder&;

private:
    rhi::TextureDesc desc_;
};

}
