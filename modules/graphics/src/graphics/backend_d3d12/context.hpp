#pragma once

#include "graphics/context.hpp"
#include "descriptor.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class FrameContextD3D12 : public FrameContext, public RefFromThis<FrameContextD3D12> {
public:
    FrameContextD3D12(Ref<class DeviceD3D12> device);
    ~FrameContextD3D12() override;

    void Reset() override;

    Ptr<class CommandEncoder> GetCommandEncoder(QueueType queue = QueueType::eGraphics) override;

    DescriptorHandle GetDescriptorSet(const DescriptorSetLayout &layout, const ShaderParams &values);

private:
    Ref<DeviceD3D12> device_;

    ComPtr<ID3D12CommandAllocator> graphics_command_allocator_;
    Vec<ComPtr<ID3D12GraphicsCommandList4>> allocated_command_lists_;
    size_t available_command_list_index_;

    Ptr<ShaderVisibleDescriptorHeapD3D12> cbv_srv_uav_heap_;
    Ptr<ShaderVisibleDescriptorHeapD3D12> sampler_heap_;

    HashMap<std::pair<DescriptorSetLayout, ShaderParams>, DescriptorHandle> descriptor_sets_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
