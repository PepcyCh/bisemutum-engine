#include "device.hpp"

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include "core/container.hpp"
#include "core/logger.hpp"
#include "queue.hpp"
#include "resource.hpp"
#include "sampler.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

namespace {

constexpr D3D_FEATURE_LEVEL MakeD3D12FeatureLevel(UINT major, UINT minor) {
    return static_cast<D3D_FEATURE_LEVEL>((major << 12) | (minor << 8));
}

}

DeviceD3D12::DeviceD3D12(const DeviceDesc &desc) {
    if (desc.enable_validation) {
        D3D12GetDebugInterface(IID_PPV_ARGS(&debug_));
        debug_->EnableDebugLayer();
    }

#ifdef BI_DEBUG_MODE
    UINT factory_flags = DXGI_CREATE_FACTORY_DEBUG;
#else
    UINT factory_flags = 0;
#endif
    CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&factory_));

    Vec<IDXGIAdapter3 *> adapters;
    {
        UINT i = 0;
        IDXGIAdapter3 *p;
        while (factory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&p))
            != DXGI_ERROR_NOT_FOUND) {
            adapters.push_back(p);
            ++i;
        }
    }
    constexpr D3D_FEATURE_LEVEL kD3D12FeatureLevel =
        MakeD3D12FeatureLevel(BISMUTH_D3D12_FEATURE_LEVEL_MAJOR, BISMUTH_D3D12_FEATURE_LEVEL_MINOR);
    IDXGIAdapter3 *selected_adapter = nullptr;
    DXGI_ADAPTER_DESC1 adapter_desc {};
    for (IDXGIAdapter3 *adapter : adapters) {
        DXGI_ADAPTER_DESC1 desc {};
        adapter->GetDesc1(&desc);
        if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) {
            continue;
        }
        if (FAILED(D3D12CreateDevice(adapter, kD3D12FeatureLevel, IID_PPV_ARGS((ID3D12Device **) 0)))) {
            continue;
        }

        selected_adapter = adapter;
        adapter_desc = desc;
        break;
    }
    if (selected_adapter == nullptr) {
        BI_CRTICAL(gGraphicsLogger, "No available GPU found");
    }
    BI_INFO(gGraphicsLogger, "Use GPU: {}", WCharsToString(adapter_desc.Description));

    D3D12CreateDevice(selected_adapter, kD3D12FeatureLevel, IID_PPV_ARGS(&device_));

    window_ = glfwGetWin32Window(desc.window);
    surface_format_ = desc.surface_format;
    raw_surface_format_ = ToDxFormat(desc.surface_format);

    D3D12MA::ALLOCATOR_DESC allocator_desc {
        .Flags = D3D12MA::ALLOCATOR_FLAG_NONE,
        .pDevice = device_.Get(),
        .pAdapter = selected_adapter,
    };
    D3D12MA::CreateAllocator(&allocator_desc, &allocator_);
}

DeviceD3D12::~DeviceD3D12() {
    allocator_->Release();
    allocator_ = nullptr;
}

Ptr<DeviceD3D12> DeviceD3D12::Create(const DeviceDesc &desc) {
    return Ptr<DeviceD3D12>::Make(desc);
}

Ptr<Queue> DeviceD3D12::GetQueue(QueueType type) {
    D3D12_COMMAND_LIST_TYPE type_dx;
    switch (type) {
        case QueueType::eGraphics:
            type_dx = D3D12_COMMAND_LIST_TYPE_DIRECT;
            break;
        case QueueType::eCompute:
            type_dx = D3D12_COMMAND_LIST_TYPE_COMPUTE;
            break;
        case QueueType::eTransfer:
            type_dx = D3D12_COMMAND_LIST_TYPE_COPY;
            break;
    }
    return Ptr<QueueD3D12>::Make(this, type_dx);
}

Ptr<Buffer> DeviceD3D12::CreateBuffer(const BufferDesc &desc) {
    return Ptr<BufferD3D12>::Make(this, desc);
}

Ptr<Texture> DeviceD3D12::CreateTexture(const TextureDesc &desc) {
    return Ptr<TextureD3D12>::Make(this, desc);
}

Ptr<Sampler> DeviceD3D12::CreateSampler(const SamplerDesc &desc) {
    return Ptr<SamplerD3D12>::Make(this, desc);
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
