#pragma once

#include <vector>

#include "resource.hpp"
#include "sampler.hpp"
#include "shader.hpp"

namespace bi::rhi {

enum class DescriptorType : uint8_t {
    none,
    sampler,
    uniform_buffer,
    read_only_storage_buffer,
    read_write_storage_buffer,
    sampled_texture,
    read_only_storage_texture,
    read_write_storage_texture,
    acceleration_structure,
    count,
};

struct BindGroupLayoutEntry final {
    uint32_t count = 1;
    DescriptorType type = DescriptorType::none;
    BitFlags<ShaderStage> visibility = ShaderStage::all_stages;
    uint32_t binding_or_register = 0;
    uint32_t space = 0;
};
using BindGroupLayout = std::vector<BindGroupLayoutEntry>;

struct StaticSampler final {
    CRef<Sampler> sampler;
    uint32_t binding_or_register = 0;
    uint32_t space = 0;
    BitFlags<ShaderStage> visibility = ShaderStage::all_stages;
};

enum class DescriptorHeapType : uint8_t {
    sampler,
    resource,
};

struct DescriptorHandle final {
    uint64_t cpu = 0;
    uint64_t gpu = 0;

    auto operator==(DescriptorHandle const& rhs) const -> bool = default;
};

struct BufferDescriptorDesc final {
    Ref<Buffer> buffer;
    DescriptorType type = DescriptorType::uniform_buffer;

    uint64_t offset = 0;
    // If `structure_stride` is 0, `size` is in bytes and descriptor is for raw buffer.
    // Otherwise, `structure_stride` is size of element in structured buffer and `size` is number of elements.
    uint64_t size = ~0ull;
    uint32_t structure_stride = 0;
    ResourceFormat format = ResourceFormat::undefined; // TODO - support texel buffer
};

struct TextureDescriptorDesc final {
    Ref<Texture> texture;
    DescriptorType type = DescriptorType::sampled_texture;

    uint32_t base_level = 0;
    uint32_t num_levels = ~0u;
    uint32_t base_layer = 0;
    uint32_t num_layers = ~0u;
    ResourceFormat format = ResourceFormat::undefined;
    TextureViewType view_type = TextureViewType::automatic;
};

struct DescriptorHeapDesc final {
    uint32_t max_count = 0;
    DescriptorHeapType type = DescriptorHeapType::resource;
    bool shader_visible = false;
};

struct DescriptorHeap {
    virtual ~DescriptorHeap() = default;

    virtual auto total_heap_size() const -> uint64_t = 0;

    virtual auto size_of_descriptor(DescriptorType type) const -> uint32_t = 0;
    virtual auto size_of_descriptor(BindGroupLayout const& layout) const -> uint32_t = 0;
    virtual auto alignment_of_descriptor(DescriptorType type) const -> uint32_t = 0;
    virtual auto alignment_of_descriptor(BindGroupLayout const& layout) const -> uint32_t = 0;

    virtual auto start_address() const -> DescriptorHandle = 0;

    virtual auto allocate_descriptor_at(DescriptorHandle handle, DescriptorType type) -> void {}
    virtual auto allocate_descriptor_at(DescriptorHandle handle, BindGroupLayout const& layout) -> void {}
    virtual auto free_descriptor_at(DescriptorHandle handle) -> void {}
    virtual auto reset() -> void {}
};

struct RenderTargetTextureDesc final {
    Ref<Texture> texture;
    uint32_t mip_level = 0;
    uint32_t base_layer = 0;
    uint32_t num_layers = 1;
    ResourceFormat format = ResourceFormat::undefined;
    TextureViewType view_type = TextureViewType::automatic;
};

}
