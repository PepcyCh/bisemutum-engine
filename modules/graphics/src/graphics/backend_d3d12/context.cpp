#include "context.hpp"

#include "device.hpp"
#include "command.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

FrameContextD3D12::FrameContextD3D12(Ref<class DeviceD3D12> device) : device_(device) {
    device->Raw()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&graphics_command_allocator_));
    available_command_list_index_ = 0;

    cbv_srv_uav_heap_ =
        Ptr<ShaderVisibleDescriptorHeapD3D12>::Make(device_, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 65536);
    sampler_heap_ = Ptr<ShaderVisibleDescriptorHeapD3D12>::Make(device_, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 2048);
}

FrameContextD3D12::~FrameContextD3D12() {}

void FrameContextD3D12::Reset() {
    graphics_command_allocator_->Reset();
}

Ptr<CommandEncoder> FrameContextD3D12::GetCommandEncoder(QueueType queue) {
    ID3D12GraphicsCommandList4 *cmd_list = nullptr;
    if (available_command_list_index_ < allocated_command_lists_.size()) {
        cmd_list = allocated_command_lists_[available_command_list_index_].Get();
        ++available_command_list_index_;

        cmd_list->Reset(graphics_command_allocator_.Get(), nullptr);
    } else {
        ComPtr<ID3D12GraphicsCommandList4> cmd_list_ptr;
        device_->Raw()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, graphics_command_allocator_.Get(), nullptr,
            IID_PPV_ARGS(&cmd_list_ptr));
        allocated_command_lists_.emplace_back(cmd_list_ptr);
        ++available_command_list_index_;
        cmd_list = allocated_command_lists_.back().Get();
    }

    ID3D12DescriptorHeap *heaps[] = { cbv_srv_uav_heap_->Raw(), sampler_heap_->Raw() };
    cmd_list->SetDescriptorHeaps(2, heaps);

    return Ptr<CommandEncoderD3D12>::Make(device_, RefThis(), cmd_list);
}

DescriptorHandle FrameContextD3D12::GetDescriptorSet(const DescriptorSetLayout &layout, const ShaderParams &values) {
    if (auto it = descriptor_sets_.find(values); it != descriptor_sets_.end()) {
        return it->second;
    }
    
    bool use_sampler_heap = false;
    for (const auto binding : layout.bindings) {
        if (binding.type == DescriptorType::eSampler) {
            use_sampler_heap = true;
            break;
        } else if (binding.type != DescriptorType::eNone) {
            break;
        }
    }

    auto descriptor_set =
        (use_sampler_heap ? sampler_heap_ : cbv_srv_uav_heap_)->AllocateAndWriteDescriptors(layout, values);
    descriptor_sets_.insert({values, descriptor_set});
    return descriptor_set;
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
