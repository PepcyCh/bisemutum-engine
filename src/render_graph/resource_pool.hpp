#pragma once

#include "core/container.hpp"
#include "mod.hpp"
#include "graphics/device.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFXRG_NAMESPACE_BEGIN

struct BufferKey {
    uint64_t size;
    BitFlags<gfx::BufferUsage> usages;
    gfx::BufferMemoryProperty memory_property;

    BufferKey(const gfx::BufferDesc &desc);

    bool operator==(const BufferKey &rhs) const = default;
};

struct TextureKey {
    gfx::Extent3D extent;
    uint32_t levels;
    gfx::ResourceFormat format;
    gfx::TextureDimension dim;
    BitFlags<gfx::TextureUsage> usages;

    TextureKey(const gfx::TextureDesc &desc);

    bool operator==(const TextureKey &rhs) const = default;
};

BISMUTH_GFXRG_NAMESPACE_END

BISMUTH_NAMESPACE_END

template <>
struct std::hash<bismuth::gfxrg::BufferKey> {
    size_t operator()(const bismuth::gfxrg::BufferKey &v) const noexcept {
        return bismuth::Hash(v.size, v.usages, v.memory_property);
    }
};

template <>
struct std::hash<bismuth::gfxrg::TextureKey> {
    size_t operator()(const bismuth::gfxrg::TextureKey &v) const noexcept {
        return bismuth::Hash(v.extent.width, v.extent.height, v.extent.depth_or_layers,
            v.levels, v.format, v.dim, v.usages);
    }
};

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFXRG_NAMESPACE_BEGIN

class ResourcePool {
public:
    ResourcePool(Ref<gfx::Device> device);

    struct Buffer {
        Ref<gfx::Buffer> buffer;
        size_t index;
        gfx::ResourceAccessType access_type;
    };
    Buffer GetBuffer(const gfx::BufferDesc &desc);
    void RemoveBuffer(const gfx::BufferDesc &desc, const Buffer &buffer);

    
    struct Texture {
        Ref<gfx::Texture> texture;
        size_t index;
        gfx::ResourceAccessType access_type;
    };
    Texture GetTexure(const gfx::TextureDesc &desc);
    void RemoveTexture(const gfx::TextureDesc &desc, const Texture &texture);

private:
    Ref<gfx::Device> device_;

    struct BufferPool {
        Vec<Ptr<gfx::Buffer>> created_buffers;
        // only updated when creating & deleting, used for initial state
        Vec<gfx::ResourceAccessType> access;
        Vec<size_t> recycled_index;
    };
    HashMap<BufferKey, BufferPool> buffer_pools_;

    struct TexturePool {
        Vec<Ptr<gfx::Texture>> created_textures;
        // only updated when creating & deleting, used for initial state
        Vec<gfx::ResourceAccessType> access;
        Vec<size_t> recycled_index;
    };
    HashMap<TextureKey, TexturePool> texture_pools_;
};

BISMUTH_GFXRG_NAMESPACE_END

BISMUTH_NAMESPACE_END
