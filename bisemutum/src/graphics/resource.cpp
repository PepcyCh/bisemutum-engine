#include <bisemutum/graphics/resource.hpp>

#include <bisemutum/engine/engine.hpp>
#include <bisemutum/prelude/hash.hpp>
#include <bisemutum/rhi/device.hpp>
#include <bisemutum/graphics/graphics_manager.hpp>
#include <bisemutum/graphics/shader_param.hpp>

auto std::hash<bi::gfx::details::BufferDescriptorKey>::operator()(
    bi::gfx::details::BufferDescriptorKey const& v
) const noexcept -> size_t {
    return bi::hash(v.type, v.format, v.offset, v.size, v.structure_stride, v.buffer_frame_index);
}

auto std::hash<bi::gfx::details::TextureDescriptorKey>::operator()(
    bi::gfx::details::TextureDescriptorKey const& v
) const noexcept -> size_t {
    return bi::hash(v.type, v.format, v.view_type, v.base_level, v.num_levels, v.base_layer, v.num_layers);
}

namespace bi::gfx {

Buffer::Buffer(rhi::BufferDesc const& desc, bool with_staging_buffer)
    : with_staging_buffer_(with_staging_buffer)
{
    if (desc.memory_property == rhi::BufferMemoryProperty::gpu_only) {
        buffer_ = g_engine->graphics_manager()->device()->create_buffer(desc);
        if (with_staging_buffer_) {
            auto staging_desc = desc;
            staging_desc.usages = {};
            staging_desc.memory_property = rhi::BufferMemoryProperty::cpu_to_gpu;
            staging_desc.persistently_mapped = true;
            staging_buffers_.resize(g_engine->graphics_manager()->num_frames_in_flight());
            for (size_t i = 0; i < staging_buffers_.size(); i++) {
                staging_buffers_[i] = g_engine->graphics_manager()->device()->create_buffer(staging_desc);
            }
        }
    } else {
        with_staging_buffer_ = false;
        staging_buffers_.resize(g_engine->graphics_manager()->num_frames_in_flight());
        for (size_t i = 0; i < staging_buffers_.size(); i++) {
            staging_buffers_[i] = g_engine->graphics_manager()->device()->create_buffer(desc);
        }
    }
}

auto Buffer::has_value() const -> bool {
    return buffer_ || !staging_buffers_.empty();
}

auto Buffer::reset() -> void {
    buffer_.reset();
    staging_buffers_.clear();
    // TODO - free cpu descriptors
    cpu_descriptors_.clear();
}

auto Buffer::rhi_buffer() -> Ref<rhi::Buffer> {
    return buffer_ ? buffer_.ref() : rhi_staging_buffer();
}
auto Buffer::rhi_buffer() const -> CRef<rhi::Buffer> {
    return buffer_ ? buffer_.ref() : rhi_staging_buffer();
}

auto Buffer::rhi_staging_buffer() -> Ref<rhi::Buffer> {
    return staging_buffers_[g_engine->graphics_manager()->curr_frame_index()].ref();
}
auto Buffer::rhi_staging_buffer() const -> CRef<rhi::Buffer> {
    return staging_buffers_[g_engine->graphics_manager()->curr_frame_index()].ref();
}

auto Buffer::set_data_raw(void const* data, uint64_t size, uint64_t offset) -> void {
    if (staging_buffers_.empty()) { return; }

    size = std::min(size, desc().size - offset);
    std::memcpy(rhi_staging_buffer()->typed_map<std::byte>() + offset, data, size);
    rhi_staging_buffer()->unmap();

    if (buffer_) {
        BitFlags<rhi::ResourceAccessType> target_access{};
        if (desc().usages.contains(rhi::BufferUsage::uniform)) {
            target_access.set(rhi::ResourceAccessType::uniform_buffer_read);
        } else if (desc().usages.contains(rhi::BufferUsage::indirect)) {
            target_access.set(rhi::ResourceAccessType::indirect_read);
        } else if (desc().usages.contains(rhi::BufferUsage::storage_read)) {
            target_access.set(rhi::ResourceAccessType::storage_resource_read);
        }

        g_engine->graphics_manager()->execute_in_this_frame(
            [this, size, offset, target_access](Ref<rhi::CommandEncoder> cmd) {
                cmd->resource_barriers({
                    rhi::BufferBarrier{
                        .buffer = buffer_.ref(),
                        .src_access_type = target_access,
                        .dst_access_type = rhi::ResourceAccessType::transfer_write,
                    },
                }, {});
                cmd->copy_buffer_to_buffer(
                    rhi_staging_buffer(), buffer_.ref(),
                    {offset, offset, size}
                );
                cmd->resource_barriers({
                    rhi::BufferBarrier{
                        .buffer = buffer_.ref(),
                        .src_access_type = rhi::ResourceAccessType::transfer_write,
                        .dst_access_type = target_access,
                    },
                }, {});
            }
        );
    }
}
auto Buffer::set_multiple_data_raw(CSpan<DataSetDesc> descs) -> void {
    if (staging_buffers_.empty()) { return; }

    auto mapped_ptr = rhi_staging_buffer()->typed_map<std::byte>();
    for (auto& data : descs) {
        auto size = std::min(data.size, desc().size - data.offset);
        std::memcpy(mapped_ptr + data.offset, data.data, size);
    }
    rhi_staging_buffer()->unmap();

    if (buffer_) {
        BitFlags<rhi::ResourceAccessType> target_access{};
        if (desc().usages.contains(rhi::BufferUsage::uniform)) {
            target_access.set(rhi::ResourceAccessType::uniform_buffer_read);
        } else if (desc().usages.contains(rhi::BufferUsage::indirect)) {
            target_access.set(rhi::ResourceAccessType::indirect_read);
        } else if (desc().usages.contains(rhi::BufferUsage::storage_read)) {
            target_access.set(rhi::ResourceAccessType::storage_resource_read);
        }

        g_engine->graphics_manager()->execute_in_this_frame(
            [this, target_access](Ref<rhi::CommandEncoder> cmd) {
                cmd->resource_barriers({
                    rhi::BufferBarrier{
                        .buffer = buffer_.ref(),
                        .src_access_type = target_access,
                        .dst_access_type = rhi::ResourceAccessType::transfer_write,
                    },
                }, {});
                cmd->copy_buffer_to_buffer(rhi_staging_buffer(), buffer_.ref(), {});
                cmd->resource_barriers({
                    rhi::BufferBarrier{
                        .buffer = buffer_.ref(),
                        .src_access_type = rhi::ResourceAccessType::transfer_write,
                        .dst_access_type = target_access,
                    },
                }, {});
            }
        );
    }
}

auto Buffer::get_cbv() -> rhi::DescriptorHandle {
    return get_descriptor(details::BufferDescriptorKey{
        .type = rhi::DescriptorType::uniform_buffer,
        .buffer_frame_index = buffer_ ? ~0u : g_engine->graphics_manager()->curr_frame_index(),
    });
}
auto Buffer::get_descriptor(
    ShaderParameterMetadata const& metadata, uint64_t offset, uint64_t size
) -> rhi::DescriptorHandle {
    return get_descriptor(details::BufferDescriptorKey{
        .type = metadata.descriptor_type,
        .format = metadata.format,
        .offset = offset,
        .size = size,
        .structure_stride = static_cast<uint32_t>(metadata.structure_stride),
        .buffer_frame_index = buffer_ ? ~0u : g_engine->graphics_manager()->curr_frame_index(),
    });
}
auto Buffer::get_descriptor(details::BufferDescriptorKey&& key) -> rhi::DescriptorHandle {
    if (auto it = cpu_descriptors_.find(key); it != cpu_descriptors_.end()) {
        return it->second;
    }

    auto handle = g_engine->graphics_manager()->cpu_resource_descriptor_heap()->allocate_descriptor(key.type);
    g_engine->graphics_manager()->device()->create_descriptor(
        rhi::BufferDescriptorDesc{
            .buffer = key.buffer_frame_index == ~0u ? buffer_.ref() : staging_buffers_[key.buffer_frame_index].ref(),
            .type = key.type,
            .offset = key.offset,
            .size = key.size,
            .structure_stride = key.structure_stride,
            .format = key.format,
        },
        handle
    );
    cpu_descriptors_.insert({std::move(key), handle});
    return handle;
}


Texture::Texture(rhi::TextureDesc const& desc) {
    texture_ = g_engine->graphics_manager()->device()->create_texture(desc);
}

Texture::Texture(Ref<rhi::Texture> imported_texture) : imported_texture_(imported_texture) {}

auto Texture::has_value() const -> bool {
    return texture_ || imported_texture_;
}

auto Texture::reset() -> void {
    texture_.reset();
    imported_texture_ = nullptr;
    cpu_descriptors_.clear();
}

auto Texture::rhi_texture() -> Ref<rhi::Texture> {
    return imported_texture_.has_value() ? imported_texture_.value() : texture_.ref();
}
auto Texture::rhi_texture() const -> CRef<rhi::Texture> {
    return imported_texture_.has_value() ? imported_texture_.value() : texture_.ref();
}

auto Texture::get_srv(uint32_t base_level, uint32_t num_levels, uint32_t base_layer, uint32_t num_layers) -> rhi::DescriptorHandle {
    return get_descriptor(details::TextureDescriptorKey{
        .type = rhi::DescriptorType::sampled_texture,
        .format = texture_->desc().format,
        .view_type = rhi::TextureViewType::automatic,
        .base_level = base_level,
        .num_levels = num_levels,
        .base_layer = base_layer,
        .num_layers = num_layers,
    });
}
auto Texture::get_uav(uint32_t mip_level, uint32_t base_layer, uint32_t num_layers) -> rhi::DescriptorHandle {
    return get_descriptor(details::TextureDescriptorKey{
        .type = rhi::DescriptorType::read_write_storage_texture,
        .format = texture_->desc().format,
        .view_type = rhi::TextureViewType::automatic,
        .base_level = mip_level,
        .num_levels = 1,
        .base_layer = base_layer,
        .num_layers = num_layers,
    });
}
auto Texture::get_descriptor(
    ShaderParameterMetadata const& metadata,
    uint32_t base_level, uint32_t num_levels,
    uint32_t base_layer, uint32_t num_layers
) -> rhi::DescriptorHandle {
    return get_descriptor(details::TextureDescriptorKey{
        .type = metadata.descriptor_type,
        .format = metadata.format,
        .view_type = metadata.texture_view_type,
        .base_level = base_level,
        .num_levels = num_levels,
        .base_layer = base_layer,
        .num_layers = num_layers,
    });
}
auto Texture::get_descriptor(details::TextureDescriptorKey&& key) -> rhi::DescriptorHandle {
    if (auto it = cpu_descriptors_.find(key); it != cpu_descriptors_.end()) {
        return it->second;
    }

    auto handle = g_engine->graphics_manager()->cpu_resource_descriptor_heap()->allocate_descriptor(key.type);
    g_engine->graphics_manager()->device()->create_descriptor(
        rhi::TextureDescriptorDesc{
            .texture = rhi_texture(),
            .type = key.type,
            .base_level = key.base_level,
            .num_levels = key.num_levels,
            .base_layer = key.base_layer,
            .num_layers = key.num_layers,
            .format = key.format,
            .view_type = key.view_type,
        },
        handle
    );
    cpu_descriptors_.insert({std::move(key), handle});
    return handle;
}

}
