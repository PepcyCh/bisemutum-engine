#pragma once

#include <bisemutum/rhi/pipeline.hpp>
#include <d3d12.h>
#include <wrl.h>

namespace bi::rhi {

struct DeviceD3D12;

struct GraphicsPipelineD3D12 final : GraphicsPipeline {
    GraphicsPipelineD3D12(Ref<DeviceD3D12> device, GraphicsPipelineDesc const& desc);

    auto raw() const -> ID3D12PipelineState* { return pipeline_.Get(); }

    auto raw_root_signature() const -> ID3D12RootSignature* { return root_signature_.Get(); }

    auto primitive_topology() const -> D3D12_PRIMITIVE_TOPOLOGY { return primitive_topology_; }

    auto root_constant_index() const -> uint32_t { return root_constant_index_; }

private:
    Ref<DeviceD3D12> device_;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_ = nullptr;

    D3D12_PRIMITIVE_TOPOLOGY primitive_topology_;
    uint32_t root_constant_index_ = 0;
};

struct ComputePipelineD3D12 final : ComputePipeline {
    ComputePipelineD3D12(Ref<DeviceD3D12> device, ComputePipelineDesc const& desc);

    auto raw() const -> ID3D12PipelineState* { return pipeline_.Get(); }

    auto raw_root_signature() const -> ID3D12RootSignature* { return root_signature_.Get(); }

    auto root_constant_index() const -> uint32_t { return root_constant_index_; }

private:
    Ref<DeviceD3D12> device_;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_ = nullptr;

    uint32_t root_constant_index_ = 0;
};

}
