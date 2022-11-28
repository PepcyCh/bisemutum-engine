#pragma once

#include "graphics/pipeline.hpp"
#include "shader.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class RenderPipelineD3D12 final : public RenderPipeline {
public:
    RenderPipelineD3D12(Ref<class DeviceD3D12> device, const RenderPipelineDesc &desc);
    ~RenderPipelineD3D12() override;

    void SetTargetFormats(Span<ResourceFormat> color_formats, ResourceFormat depth_stencil_format);

    ID3D12PipelineState *RawPipeline() const { return pipeline_.Get(); }

    ID3D12RootSignature *RawRootSignature() const { return root_signature_.Get(); }

    D3D12_PRIMITIVE_TOPOLOGY RawPrimitiveTopology() const { return topology_; }

    const RenderPipelineDesc &Desc() const { return desc_; }

private:
    Ref<DeviceD3D12> device_;
    RenderPipelineDesc desc_;

    ComPtr<ID3D12RootSignature> root_signature_;
    ComPtr<ID3D12PipelineState> pipeline_;

    D3D12_PRIMITIVE_TOPOLOGY topology_;
};

class ComputePipelineD3D12 final : public ComputePipeline {
public:
    ComputePipelineD3D12(Ref<class DeviceD3D12> device, const ComputePipelineDesc &desc);
    ~ComputePipelineD3D12() override;

    uint32_t LocalSizeX() const { return desc_.thread_group.x; }
    uint32_t LocalSizeY() const { return desc_.thread_group.y; }
    uint32_t LocalSizeZ() const { return desc_.thread_group.z; }

    ID3D12PipelineState *RawPipeline() const { return pipeline_.Get(); }

    ID3D12RootSignature *RawRootSignature() const { return root_signature_.Get(); }

    const ComputePipelineDesc &Desc() const { return desc_; }

private:
    Ref<DeviceD3D12> device_;
    ComputePipelineDesc desc_;

    ComPtr<ID3D12RootSignature> root_signature_;
    ComPtr<ID3D12PipelineState> pipeline_;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
