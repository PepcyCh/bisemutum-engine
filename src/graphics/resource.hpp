#pragma once

#include <string>

#include "defines.hpp"
#include "core/bitflags.hpp"
#include "core/ptr.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

enum class ResourceAccessType : uint32_t {
    eNone = 0x0,
    eVertexBufferRead = 0x1,
    eIndexBufferRead = 0x2,
    eIndirectRead = 0x4,
    eRenderShaderUniformBufferRead = 0x8,
    eRenderShaderSampledTextureRead = 0x10,
    eRenderShaderStorageResourceRead = 0x20,
    eRenderShaderStorageResourceWrite = 0x40,
    eComputeShaderUniformBufferRead = 0x80,
    eComputeShaderSampledTextureRead = 0x100,
    eComputeShaderStorageResourceRead = 0x200,
    eComputeShaderStorageResourceWrite = 0x400,
    eColorAttachmentRead = 0x800,
    eColorAttachmentWrite = 0x1000,
    eDepthStencilAttachmentRead = 0x2000,
    eDepthStencilAttachmentWrite = 0x4000,
    eTransferRead = 0x8000,
    eTransferWrite = 0x10000,
    eResolveRead = 0x20000,
    eResolveWrite = 0x40000,
    ePresent = 0x80000,
};

enum class BufferUsage : uint8_t {
    eNone = 0x0,
    eVertex = 0x1,
    eIndex = 0x2,
    eUniform = 0x4,
    eStorage = 0x8,
    eRWStorage = 0x10 | 0x8,
    eIndirect = 0x20,
    eAccelerationStructure = 0x40,
};

enum class BufferMemoryProperty : uint8_t {
    eGpuOnly,
    eCpuToGpu,
    eGpuToCpu,
};

struct BufferDesc {
    std::string name = "";
    uint64_t size = 0;
    BitFlags<BufferUsage> usages = BufferUsage::eNone;
    BufferMemoryProperty memory_property = BufferMemoryProperty::eGpuOnly;
    bool persistently_mapped = false;
};

class Buffer {
public:
    virtual ~Buffer() = default;

    virtual void *Map() = 0;

    virtual void Unmap() = 0;

protected:
    Buffer() = default;
};

struct BufferRange {
    Ref<Buffer> buffer;
    uint64_t offset = 0;
    uint64_t length = ~0ull;

    bool operator==(const BufferRange &rhs) const = default;
};

enum class TextureUsage : uint8_t {
    eNone = 0x0,
    eSampled = 0x1,
    eStorage = 0x2,
    eRWStorage = 0x4 | 0x2,
    eColorAttachment = 0x8,
    eDepthStencilAttachment = 0x10,
};

enum class TextureDimension : uint8_t {
    e1D,
    e2D,
    e3D,
};

struct TextureDesc {
    std::string name = "";
    Extent3D extent = {};
    uint32_t levels = 1;
    ResourceFormat format = ResourceFormat::eUndefined;
    TextureDimension dim = TextureDimension::e2D;
    BitFlags<TextureUsage> usages = TextureUsage::eNone;
};

class Texture {
public:
    virtual ~Texture() = default;

protected:
    Texture() = default;
};

enum class TextureViewDimension : uint8_t {
    e1D,
    e1DArray,
    e2D,
    e2DArray,
    eCube,
    eCubeArray,
    e3D,
};

struct TextureView {
    Ref<Texture> texture;
    uint32_t base_level = 0;
    uint32_t levels = ~0u;
    uint32_t base_layer = 0;
    uint32_t layers = ~0u;

    bool operator==(const TextureView &rhs) const = default;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
