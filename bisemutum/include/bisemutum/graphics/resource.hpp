#pragma once

#include <unordered_map>

#include "../prelude/box.hpp"
#include "../rhi/descriptor.hpp"

namespace bi::gfx::details {

struct BufferDescriptorKey final {
    rhi::DescriptorType type = rhi::DescriptorType::uniform_buffer;
    rhi::ResourceFormat format = rhi::ResourceFormat::undefined;
    uint64_t offset = 0;
    uint64_t size = ~0ull;
    uint32_t structure_stride = 0;
    uint32_t buffer_frame_index = ~0u;

    auto operator==(BufferDescriptorKey const& rhs) const -> bool = default;
};

struct TextureDescriptorKey final {
    rhi::DescriptorType type = rhi::DescriptorType::sampled_texture;
    rhi::ResourceFormat format = rhi::ResourceFormat::undefined;
    rhi::TextureViewType view_type = rhi::TextureViewType::automatic;
    uint32_t base_level = 0;
    uint32_t num_levels = ~0u;
    uint32_t base_layer = 0;
    uint32_t num_layers = ~0u;

    auto operator==(TextureDescriptorKey const& rhs) const -> bool = default;
};

}

template <>
struct std::hash<bi::gfx::details::BufferDescriptorKey> final {
    auto operator()(bi::gfx::details::BufferDescriptorKey const& v) const noexcept -> size_t;
};

template <>
struct std::hash<bi::gfx::details::TextureDescriptorKey> final {
    auto operator()(bi::gfx::details::TextureDescriptorKey const& v) const noexcept -> size_t;
};

namespace bi::gfx {

struct ShaderParameterMetadata;

struct Buffer final {
    Buffer() = default;
    Buffer(rhi::BufferDesc const& desc, bool with_staging_buffer = true);
    ~Buffer();

    Buffer(Buffer&& rhs) noexcept;
    auto operator=(Buffer&& rhs) noexcept -> Buffer&;

    auto has_value() const -> bool;
    auto reset() -> void;

    auto desc() const -> rhi::BufferDesc const& { return rhi_buffer()->desc(); }

    auto rhi_buffer() -> Ref<rhi::Buffer>;
    auto rhi_buffer() const -> CRef<rhi::Buffer>;

    auto update_staging_buffer_size() -> void;

    template <typename T>
    auto set_data(T const* data, uint64_t count = 1, uint64_t offset = 0) -> void {
        set_data_raw(data, count * sizeof(T), offset);
    }
    auto set_data_raw(void const* data, uint64_t size, uint64_t offset = 0) -> void;

    struct DataSetDesc final {
        void const* data;
        uint64_t size;
        uint64_t offset;
    };
    auto set_multiple_data_raw(CSpan<DataSetDesc> descs) -> void;

    auto get_cbv() -> rhi::DescriptorHandle;
    auto get_descriptor(
        ShaderParameterMetadata const& metadata,
        uint64_t offset = 0, uint64_t size = ~0ull
    ) -> rhi::DescriptorHandle;

private:
    auto rhi_staging_buffer() -> Ref<rhi::Buffer>;
    auto rhi_staging_buffer() const -> CRef<rhi::Buffer>;

    auto get_descriptor(details::BufferDescriptorKey&& key) -> rhi::DescriptorHandle;

    auto free_all_cpu_descriptors() -> void;
    auto free_cpu_descriptors_at_frame(uint32_t frame_index) -> void;

    Box<rhi::Buffer> buffer_;
    std::vector<Box<rhi::Buffer>> staging_buffers_;
    uint64_t desired_size_;
    uint32_t staging_buffer_start_index_;
    bool with_staging_buffer_;

    std::unordered_map<details::BufferDescriptorKey, rhi::DescriptorHandle> cpu_descriptors_;
};

struct Texture final {
    Texture() = default;
    Texture(rhi::TextureDesc const& desc);
    ~Texture();

    Texture(Texture&& rhs) noexcept;
    auto operator=(Texture&& rhs) noexcept -> Texture&;

    auto has_value() const -> bool;
    auto reset() -> void;

    auto desc() const -> rhi::TextureDesc const& { return rhi_texture()->desc(); }

    auto rhi_texture() -> Ref<rhi::Texture>;
    auto rhi_texture() const -> CRef<rhi::Texture>;

    auto get_srv(
        uint32_t base_level = 0, uint32_t num_levels = ~0u,
        uint32_t base_layer = 0, uint32_t num_layers = ~0u
    ) -> rhi::DescriptorHandle;
    auto get_uav(
        uint32_t mip_level = 0, uint32_t base_layer = 0, uint32_t num_layers = ~0u
    ) -> rhi::DescriptorHandle;
    auto get_descriptor(
        ShaderParameterMetadata const& metadata,
        uint32_t base_level = 0, uint32_t num_levels = ~0u,
        uint32_t base_layer = 0, uint32_t num_layers = ~0u
    ) -> rhi::DescriptorHandle;

private:
    auto get_descriptor(details::TextureDescriptorKey&& key) -> rhi::DescriptorHandle;

    friend struct GraphicsManager;
    Texture(Ref<rhi::Texture> imported_texture);

    Box<rhi::Texture> texture_;
    // Only for imported swapchain texture.
    Ptr<rhi::Texture> imported_texture_;

    std::unordered_map<details::TextureDescriptorKey, rhi::DescriptorHandle> cpu_descriptors_;
};

}
