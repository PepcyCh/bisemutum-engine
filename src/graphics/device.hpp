#pragma once

#include <memory>

#include "defines.hpp"
#include "queue.hpp"
#include "swap_chain.hpp"
#include "sync.hpp"
#include "resource.hpp"
#include "sampler.hpp"
#include "shader.hpp"
#include "pipeline.hpp"
#include "context.hpp"

struct GLFWwindow;

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

struct DeviceDesc {
    GraphicsBackend backend = GraphicsBackend::eVulkan;
    bool enable_validation = false;
    GLFWwindow *window = nullptr;
    ResourceFormat surface_format = ResourceFormat::eBgra8Srgb;
};

class Device {
public:
    virtual ~Device() = default;

    static Ptr<Device> Create(const DeviceDesc &desc);

    virtual Ptr<Queue> GetQueue(QueueType type) = 0;

    virtual Ptr<SwapChain> CreateSwapChain(const SwapChainDesc &desc) = 0;

    virtual Ptr<Fence> CreateFence() = 0;

#ifdef CreateSemaphore
#undef CreateSemaphore
#endif
    virtual Ptr<Semaphore> CreateSemaphore() = 0;

    virtual Ptr<Buffer> CreateBuffer(const BufferDesc &desc) = 0;

    virtual Ptr<Texture> CreateTexture(const TextureDesc &desc) = 0;

    virtual Ptr<Sampler> CreateSampler(const SamplerDesc &desc) = 0;

    virtual Ptr<ShaderModule> CreateShaderModule(const Vec<uint8_t> &src_bytes) = 0;

    virtual Ptr<RenderPipeline> CreateRenderPipeline(const RenderPipelineDesc &desc) = 0;

    virtual Ptr<ComputePipeline> CreateComputePipeline(const ComputePipelineDesc &desc) = 0;

    virtual Ptr<FrameContext> CreateFrameContext() = 0;

protected:
    Device() = default;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
