#include "render_graph/resource_pool.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

ResourcePool::ResourcePool(Ref<gfx::Device> device) : device_(device) {}

BufferKey::BufferKey(const gfx::BufferDesc &desc) {
    size = desc.size;
    usages = desc.usages;
    memory_property = desc.memory_property;
}

TextureKey::TextureKey(const gfx::TextureDesc &desc) {
    extent = desc.extent;
    levels = desc.levels;
    format = desc.format;
    dim = desc.dim;
    usages = desc.usages;
}

ResourcePool::Buffer ResourcePool::GetBuffer(const gfx::BufferDesc &desc) {
    auto &pool = buffer_pools_[desc];
    if (!pool.recycled_index.empty()) {
        size_t index = pool.recycled_index.back();
        auto buffer_ref = pool.created_buffers[index].AsRef();
        auto access = pool.access[index];
        pool.recycled_index.pop_back();
        return Buffer {
            .buffer = buffer_ref,
            .index = index,
            .access_type = access,
        };
    } else {
        auto buffer = device_->CreateBuffer(desc);
        auto buffer_ref = buffer.AsRef();
        pool.created_buffers.emplace_back(std::move(buffer));
        pool.access.push_back(gfx::ResourceAccessType::eNone);
        return Buffer {
            .buffer = buffer_ref,
            .index = pool.created_buffers.size() - 1,
            .access_type = gfx::ResourceAccessType::eNone,
        };
    }
}

void ResourcePool::RemoveBuffer(const gfx::BufferDesc &desc, const Buffer &buffer) {
    auto &pool = buffer_pools_[desc];
    pool.recycled_index.push_back(buffer.index);
    pool.access[buffer.index] = buffer.access_type;
}

ResourcePool::Texture ResourcePool::GetTexure(const gfx::TextureDesc &desc) {
    auto &pool = texture_pools_[desc];
    if (!pool.recycled_index.empty()) {size_t index = pool.recycled_index.back();
        auto texture_ref = pool.created_textures[index].AsRef();
        auto access = pool.access[index];
        pool.recycled_index.pop_back();
        return Texture {
            .texture = texture_ref,
            .index = index,
            .access_type = access,
        };
    } else {
        auto texture = device_->CreateTexture(desc);
        auto texture_ref = texture.AsRef();
        pool.created_textures.emplace_back(std::move(texture));
        pool.access.push_back(gfx::ResourceAccessType::eNone);
        return Texture {
            .texture = texture_ref,
            .index = pool.created_textures.size() - 1,
            .access_type = gfx::ResourceAccessType::eNone,
        };
    }
}

void ResourcePool::RemoveTexture(const gfx::TextureDesc &desc, const Texture &texture) {
    auto &pool = texture_pools_[desc];
    pool.recycled_index.push_back(texture.index);
    pool.access[texture.index] = texture.access_type;
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
