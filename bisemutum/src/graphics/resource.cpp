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
    desired_size_ = desc.size;
    staging_buffer_start_index_ = g_engine->graphics_manager()->curr_frame_index();
    if (desc.memory_property == rhi::BufferMemoryProperty::gpu_only) {
        buffer_ = g_engine->graphics_manager()->device()->create_buffer(desc);
        if (with_staging_buffer_) {
            auto staging_desc = desc;
            staging_desc.usages = {};
            staging_desc.memory_property = rhi::BufferMemoryProperty::cpu_to_gpu;
            staging_desc.persistently_mapped = true;
            staging_buffers_.resize(g_engine->graphics_manager()->num_frames_in_flight());
            staging_buffers_[staging_buffer_start_index_] =
                g_engine->graphics_manager()->device()->create_buffer(staging_desc);
        }
    } else {
        with_staging_buffer_ = false;
        staging_buffers_.resize(g_engine->graphics_manager()->num_frames_in_flight());
        staging_buffers_[staging_buffer_start_index_] = g_engine->graphics_manager()->device()->create_buffer(desc);
    }
}

Buffer::~Buffer() {
    reset();
}

Buffer::Buffer(Buffer&& rhs) noexcept
    : buffer_(std::move(rhs.buffer_))
    , staging_buffers_(std::move(rhs.staging_buffers_))
    , desired_size_(rhs.desired_size_)
    , staging_buffer_start_index_(rhs.staging_buffer_start_index_)
    , with_staging_buffer_(rhs.with_staging_buffer_)
    , cpu_descriptors_(std::move(rhs.cpu_descriptors_))
{}

auto Buffer::operator=(Buffer&& rhs) noexcept -> Buffer& {
    reset();
    buffer_ = std::move(rhs.buffer_);
    staging_buffers_ = std::move(rhs.staging_buffers_);
    desired_size_ = rhs.desired_size_;
    staging_buffer_start_index_ = rhs.staging_buffer_start_index_;
    with_staging_buffer_ = rhs.with_staging_buffer_;
    cpu_descriptors_ = std::move(rhs.cpu_descriptors_);
    return *this;
}

auto Buffer::has_value() const -> bool {
    return buffer_ || !staging_buffers_.empty();
}

auto Buffer::reset() -> void {
    if (has_value()) {
        g_engine->graphics_manager()->add_delayed_destroy(
            [
                buffer = std::move(buffer_),
                staging_buffers = std::move(staging_buffers_),
                cpu_descriptors = std::move(cpu_descriptors_)
            ]() {
                for (auto& [_, desc] : cpu_descriptors) {
                    g_engine->graphics_manager()->free_cpu_resource_descriptor(desc);
                }
            }
        );
    }
}

auto Buffer::rhi_buffer() -> Ref<rhi::Buffer> {
    return buffer_ ? buffer_.ref() : staging_buffers_[staging_buffer_start_index_].ref();
}
auto Buffer::rhi_buffer() const -> CRef<rhi::Buffer> {
    return buffer_ ? buffer_.ref() : staging_buffers_[staging_buffer_start_index_].ref();
}

auto Buffer::rhi_staging_buffer() -> Ref<rhi::Buffer> {
    return staging_buffers_[g_engine->graphics_manager()->curr_frame_index()].ref();
}
auto Buffer::rhi_staging_buffer() const -> CRef<rhi::Buffer> {
    return staging_buffers_[g_engine->graphics_manager()->curr_frame_index()].ref();
}

auto Buffer::update_staging_buffer_size() -> void {
    if (buffer_ && !with_staging_buffer_) { return; }

    auto index = g_engine->graphics_manager()->curr_frame_index();
    auto& staging_buffer = staging_buffers_[index];
    if (!staging_buffer || staging_buffer->desc().size != desired_size_) {
        free_cpu_descriptors_at_frame(index);
        staging_buffer = g_engine->graphics_manager()->device()->create_buffer(
            staging_buffers_[staging_buffer_start_index_]->desc()
        );
    }
}

auto Buffer::set_data_raw_impl(void const* data, uint64_t size, uint64_t offset, bool immediately) -> void {
    if (staging_buffers_.empty() || size == 0) { return; }
    update_staging_buffer_size();

    size = std::min(size, desc().size - offset);
    std::memcpy(rhi_staging_buffer()->typed_map<std::byte>() + offset, data, size);
    rhi_staging_buffer()->unmap();

    if (buffer_) {
        BitFlags<rhi::ResourceAccessType> target_access{};
        if (desc().usages.contains_any(rhi::BufferUsage::uniform)) {
            target_access.set(rhi::ResourceAccessType::uniform_buffer_read);
        } else if (desc().usages.contains_any(rhi::BufferUsage::indirect)) {
            target_access.set(rhi::ResourceAccessType::indirect_read);
        } else if (desc().usages.contains_any(rhi::BufferUsage::vertex)) {
            target_access.set(rhi::ResourceAccessType::vertex_buffer_read);
        } else if (desc().usages.contains_any(rhi::BufferUsage::index)) {
            target_access.set(rhi::ResourceAccessType::index_buffer_read);
        } else if (desc().usages.contains_any(rhi::BufferUsage::storage_read)) {
            target_access.set(rhi::ResourceAccessType::storage_resource_read);
        }

        auto execute_func = [this, size, offset, target_access](Ref<rhi::CommandEncoder> cmd) {
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
        };
        if (immediately) {
            g_engine->graphics_manager()->execute_immediately(std::move(execute_func));
        } else {
            g_engine->graphics_manager()->execute_in_this_frame(std::move(execute_func));
        }
    }
}
auto Buffer::set_multiple_data_raw(CSpan<DataSetDesc> descs, bool immediately) -> void {
    if (staging_buffers_.empty()) { return; }
    update_staging_buffer_size();

    auto mapped_ptr = rhi_staging_buffer()->typed_map<std::byte>();
    for (auto& data : descs) {
        auto size = std::min(data.size, desc().size - data.offset);
        std::memcpy(mapped_ptr + data.offset, data.data, size);
    }
    rhi_staging_buffer()->unmap();

    if (buffer_) {
        BitFlags<rhi::ResourceAccessType> target_access{};
        if (desc().usages.contains_any(rhi::BufferUsage::uniform)) {
            target_access.set(rhi::ResourceAccessType::uniform_buffer_read);
        } else if (desc().usages.contains_any(rhi::BufferUsage::indirect)) {
            target_access.set(rhi::ResourceAccessType::indirect_read);
        } else if (desc().usages.contains_any(rhi::BufferUsage::storage_read)) {
            target_access.set(rhi::ResourceAccessType::storage_resource_read);
        }

        auto execute_func = [this, target_access](Ref<rhi::CommandEncoder> cmd) {
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
        };
        if (immediately) {
            g_engine->graphics_manager()->execute_immediately(std::move(execute_func));
        } else {
            g_engine->graphics_manager()->execute_in_this_frame(std::move(execute_func));
        }
    }
}

auto Buffer::get_data_raw(void* dst_data, uint64_t size, uint64_t offset) -> void {
    if (staging_buffers_.empty() || size == 0) { return; }
    update_staging_buffer_size();

    if (buffer_) {
        BitFlags<rhi::ResourceAccessType> target_access{};
        if (desc().usages.contains_any(rhi::BufferUsage::uniform)) {
            target_access.set(rhi::ResourceAccessType::uniform_buffer_read);
        } else if (desc().usages.contains_any(rhi::BufferUsage::indirect)) {
            target_access.set(rhi::ResourceAccessType::indirect_read);
        } else if (desc().usages.contains_any(rhi::BufferUsage::vertex)) {
            target_access.set(rhi::ResourceAccessType::vertex_buffer_read);
        } else if (desc().usages.contains_any(rhi::BufferUsage::index)) {
            target_access.set(rhi::ResourceAccessType::index_buffer_read);
        } else if (desc().usages.contains_any(rhi::BufferUsage::storage_read)) {
            target_access.set(rhi::ResourceAccessType::storage_resource_read);
        }

        g_engine->graphics_manager()->execute_immediately(
            [this, size, offset, target_access](Ref<rhi::CommandEncoder> cmd) {
                cmd->resource_barriers({
                    rhi::BufferBarrier{
                        .buffer = buffer_.ref(),
                        .src_access_type = target_access,
                        .dst_access_type = rhi::ResourceAccessType::transfer_read,
                    },
                }, {});
                cmd->copy_buffer_to_buffer(
                    buffer_.ref(), rhi_staging_buffer(),
                    {offset, offset, size}
                );
                cmd->resource_barriers({
                    rhi::BufferBarrier{
                        .buffer = buffer_.ref(),
                        .src_access_type = rhi::ResourceAccessType::transfer_read,
                        .dst_access_type = target_access,
                    },
                }, {});
            }
        );
    }

    size = std::min(size, desc().size - offset);
    std::memcpy(dst_data, rhi_staging_buffer()->typed_map<std::byte>() + offset, size);
    rhi_staging_buffer()->unmap();
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

    auto handle = g_engine->graphics_manager()->allocate_cpu_descriptor(key.type);
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

auto Buffer::free_all_cpu_descriptors() -> void {
    for (auto& [_, desc] : cpu_descriptors_) {
        g_engine->graphics_manager()->free_cpu_resource_descriptor(desc);
    }
    cpu_descriptors_.clear();
}
auto Buffer::free_cpu_descriptors_at_frame(uint32_t frame_index) -> void {
    std::vector<decltype(cpu_descriptors_)::iterator> deleted_descriptors;
    for (auto it = cpu_descriptors_.begin(); it != cpu_descriptors_.end(); ++it) {
        if (it->first.buffer_frame_index == frame_index) {
            deleted_descriptors.push_back(it);
            g_engine->graphics_manager()->free_cpu_resource_descriptor(it->second);
        }
    }
    for (auto it : deleted_descriptors) {
        cpu_descriptors_.erase(it);
    }
}


Texture::Texture(rhi::TextureDesc const& desc) {
    texture_ = g_engine->graphics_manager()->device()->create_texture(desc);
}

Texture::Texture(Ref<rhi::Texture> imported_texture) : imported_texture_(imported_texture) {}

Texture::~Texture() {
    reset();
}

Texture::Texture(Texture&& rhs) noexcept
    : texture_(std::move(rhs.texture_))
    , imported_texture_(rhs.imported_texture_)
    , cpu_descriptors_(std::move(rhs.cpu_descriptors_))
{}

auto Texture::operator=(Texture&& rhs) noexcept -> Texture& {
    reset();
    texture_ = std::move(rhs.texture_);
    imported_texture_ = rhs.imported_texture_;
    cpu_descriptors_ = std::move(rhs.cpu_descriptors_);
    return *this;
}

auto Texture::has_value() const -> bool {
    return texture_ || imported_texture_;
}

auto Texture::reset() -> void {
    imported_texture_ = nullptr;

    if (texture_) {
        g_engine->graphics_manager()->add_delayed_destroy(
            [
                texture = std::move(texture_),
                cpu_descriptors = std::move(cpu_descriptors_)
            ]() {
                for (auto& [_, desc] : cpu_descriptors) {
                    g_engine->graphics_manager()->free_cpu_resource_descriptor(desc);
                }
            }
        );
    }
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

    auto handle = g_engine->graphics_manager()->allocate_cpu_descriptor(key.type);
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
