#pragma once

#include <mutex>

#include <bisemutum/rhi/pipeline.hpp>
#include <bisemutum/runtime/vfs.hpp>
#include <bisemutum/prelude/span.hpp>
#include <bisemutum/containers/hash.hpp>

#include "utils.hpp"

namespace bi::rhi {

struct PipelineStateCacheD3D12 final {
    auto init(IDXGIAdapter* adapter, ID3D12Device* device) -> void;

    auto read_from(Dyn<rt::IFile>::Ref file) -> void;
    auto write_to(Dyn<rt::IFile>::Ref file) const -> void;

    auto create_graphics_pipeline(
        GraphicsPipelineDesc const& desc,
        D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc_dx,
        Microsoft::WRL::ComPtr<ID3D12PipelineState>& pipeline
    ) -> HRESULT;

    auto create_compute_pipeline(
        ComputePipelineDesc const& desc,
        D3D12_COMPUTE_PIPELINE_STATE_DESC& desc_dx,
        Microsoft::WRL::ComPtr<ID3D12PipelineState>& pipeline
    ) -> HRESULT;

private:
    IDXGIAdapter* adapter_ = nullptr;
    ID3D12Device* device_ = nullptr;

    std::vector<std::byte> data_;
    StringHashMap<std::pair<size_t, size_t>> cached_pipeline_offsets_;
    std::mutex cache_mutex_;
};

}
