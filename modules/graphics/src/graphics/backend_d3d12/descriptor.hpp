#pragma once

#include "graphics/descriptor.hpp"
#include "utils.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

struct DescriptorHandle {
    D3D12_CPU_DESCRIPTOR_HANDLE cpu;
    D3D12_GPU_DESCRIPTOR_HANDLE gpu;
};

class DescriptorHeapD3D12 {
public:
    DescriptorHeapD3D12(Ref<class DeviceD3D12> device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT max_count,
        bool shader_visible = false);
    ~DescriptorHeapD3D12();

    DescriptorHandle AllocateDecriptor();

    ID3D12DescriptorHeap *Raw() const { return heap_.Get(); }
    D3D12_DESCRIPTOR_HEAP_TYPE RawType() const { return type_; }

protected:
    Ref<DeviceD3D12> device_;
    ComPtr<ID3D12DescriptorHeap> heap_;
    D3D12_DESCRIPTOR_HEAP_TYPE type_;
    DescriptorHandle start_handle_;
    bool shader_visible_;
    UINT descriptor_size_;
    UINT max_count_;
    UINT used_count_;
};

class ShaderVisibleDescriptorHeapD3D12 : public DescriptorHeapD3D12 {
public:
    ShaderVisibleDescriptorHeapD3D12(Ref<class DeviceD3D12> device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT max_count)
        : DescriptorHeapD3D12(device, type, max_count, true) {}

    DescriptorHandle AllocateAndWriteDescriptors(const DescriptorSetLayout &layout, const ShaderParams &values);
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
