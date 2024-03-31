#pragma once

#include "defines.hpp"
#include "../prelude/bitflags.hpp"

namespace bi::rhi {

enum class ResourceAccessType : uint32_t {
    none = 0x0,
    vertex_buffer_read = 0x1,
    index_buffer_read = 0x2,
    indirect_read = 0x4,
    uniform_buffer_read = 0x8,
    sampled_texture_read = 0x10,
    storage_resource_read = 0x20,
    storage_resource_write = 0x40,
    color_attachment_read = 0x800,
    color_attachment_write = 0x1000,
    depth_stencil_attachment_read = 0x2000,
    depth_stencil_attachment_write = 0x4000,
    transfer_read = 0x8000,
    transfer_write = 0x10000,
    resolve_read = 0x20000,
    resolve_write = 0x40000,
    acceleration_structure_read = 0x80000,
    acceleration_structure_write = 0x100000,
    acceleration_structure_build_emit_data_write = 0x200000,
    shader_binding_table_read = 0x400000,
    present = 0x800000,
};

enum class BufferUsage : uint32_t {
    none = 0x0,
    vertex = 0x1,
    index = 0x2,
    uniform = 0x4,
    storage_read = 0x8,
    storage_read_write = 0x10 | 0x8,
    indirect = 0x20,
    acceleration_structure = 0x40,
    acceleration_structure_build = 0x80,
    shader_binding_table = 0x100,
};

enum class BufferMemoryProperty : uint8_t {
    gpu_only,
    cpu_to_gpu,
    gpu_to_cpu,
};

struct BufferDesc final {
    uint64_t size = 0;
    BitFlags<BufferUsage> usages = BufferUsage::none;
    BufferMemoryProperty memory_property = BufferMemoryProperty::gpu_only;
    bool persistently_mapped = false;

    auto operator==(BufferDesc const& rhs) const -> bool = default;
};

struct Buffer {
    virtual ~Buffer() = default;

    auto desc() const -> BufferDesc const& { return desc_; }

    virtual auto map() -> void* = 0;
    template <typename T>
    auto typed_map() -> T* { return static_cast<T*>(map()); }

    virtual auto unmap() -> void = 0;

protected:
    BufferDesc desc_;
};


enum class TextureUsage : uint8_t {
    none = 0x0,
    sampled = 0x1,
    storage_read = 0x2,
    storage_read_write = 0x4 | 0x2,
    color_attachment = 0x8,
    depth_stencil_attachment = 0x10,
};

enum class TextureDimension : uint8_t {
    d1,
    d2,
    d3,
};

struct TextureDesc final {
    Extent3D extent = {};
    uint32_t levels = 1;
    ResourceFormat format = ResourceFormat::undefined;
    TextureDimension dim = TextureDimension::d2;
    BitFlags<TextureUsage> usages = TextureUsage::none;

    auto operator==(TextureDesc const& rhs) const -> bool = default;
};

enum class TextureViewType : uint8_t {
    d1,
    d1_array,
    d2,
    d2_array,
    cube,
    cube_array,
    d3,
    automatic = 0xff,
};

struct Texture {
    virtual ~Texture() = default;

    auto desc() const -> TextureDesc const& { return desc_; }

    auto get_automatic_view_type() const -> TextureViewType;

protected:
    TextureDesc desc_;
};

}
