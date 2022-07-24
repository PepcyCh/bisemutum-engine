#pragma once

#include <memory>

#include "defines.hpp"
#include "queue.hpp"
#include "resource.hpp"
#include "sampler.hpp"

struct GLFWwindow;

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

enum class DeviceBackend : uint8_t {
    eVulkan,
    eD3D12,
};

struct DeviceDesc {
    DeviceBackend backend = DeviceBackend::eVulkan;
    bool enable_validation = false;
    GLFWwindow *window = nullptr;
    ResourceFormat surface_format = ResourceFormat::eBgra8Srgb;
};

class Device {
public:
    virtual ~Device() = default;

    static Ptr<Device> CreateDevice(const DeviceDesc &desc);

    virtual Ptr<Queue> GetQueue(QueueType type) = 0;

    virtual Ptr<Buffer> CreateBuffer(const struct BufferDesc &desc) = 0;

    virtual Ptr<Texture> CreateTexture(const struct TextureDesc &desc) = 0;

    virtual Ptr<Sampler> CreateSampler(const struct SamplerDesc &desc) = 0;

protected:
    Device() = default;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
