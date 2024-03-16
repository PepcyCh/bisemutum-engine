#include "pipeline_cache.hpp"

#include <bisemutum/rhi/pipeline.hpp>
#include <bisemutum/utils/crypto.hpp>
#include <bisemutum/prelude/byte_stream.hpp>
#include <fmt/format.h>

namespace bi::rhi {

namespace {

auto hash_bind_groups_layout(std::vector<BindGroupLayout> const& v) -> size_t {
    return hash_linear_container(v);
}

auto hash_static_samplers(std::vector<StaticSampler> const& v) -> size_t {
    return hash_linear_container(v);
}

auto hash_push_constants(PushConstantsDesc const& v) -> size_t {
    return hash(v);
}

auto hash_graphics_pipeline_desc(
    GraphicsPipelineDesc const& desc, D3D12_GRAPHICS_PIPELINE_STATE_DESC const& desc_dx
) -> std::string {
    auto config_hash = hash(
        hash_bind_groups_layout(desc.bind_groups_layout),
        hash_static_samplers(desc.static_samplers),
        hash_push_constants(desc.push_constants)
    );

    config_hash = hash_combine(config_hash, desc.vertex_input_buffers.size());
    for (auto const& vi : desc.vertex_input_buffers) {
        config_hash = hash_combine(config_hash, hash(vi.per_instance, vi.stride, vi.attributes.size()));
        for (auto const& attrib : vi.attributes) {
            config_hash = hash_combine(config_hash, hash(attrib.format, attrib.semantics, attrib.offset));
        }
    }

    // TODO - hash tessellation_state
    // config_hash = hash_combine(config_hash, hash_by_byte(desc.tessellation_state));
    config_hash = hash_combine(config_hash, hash_by_byte(desc.rasterization_state));

    config_hash = hash_combine(config_hash, hash_by_byte(desc.color_target_state.blend_constants));
    config_hash = hash_combine(config_hash, desc.color_target_state.attachments.size());
    for (auto const& att : desc.color_target_state.attachments) {
        config_hash = hash_combine(config_hash, hash_by_byte(att));
    }
    config_hash = hash_combine(config_hash, hash_by_byte(desc.depth_stencil_state));

    auto vs_md5 = crypto::md5({reinterpret_cast<std::byte const*>(desc_dx.VS.pShaderBytecode), desc_dx.VS.BytecodeLength});
    auto hs_md5 = crypto::md5({reinterpret_cast<std::byte const*>(desc_dx.HS.pShaderBytecode), desc_dx.HS.BytecodeLength});
    auto ds_md5 = crypto::md5({reinterpret_cast<std::byte const*>(desc_dx.DS.pShaderBytecode), desc_dx.DS.BytecodeLength});
    auto gs_md5 = crypto::md5({reinterpret_cast<std::byte const*>(desc_dx.GS.pShaderBytecode), desc_dx.GS.BytecodeLength});
    auto ps_md5 = crypto::md5({reinterpret_cast<std::byte const*>(desc_dx.PS.pShaderBytecode), desc_dx.PS.BytecodeLength});

    return fmt::format(
        "GRAPHICS-{:x}-{}-{}-{}-{}-{}",
        config_hash, vs_md5.as_string(), hs_md5.as_string(), ds_md5.as_string(), gs_md5.as_string(), ps_md5.as_string()
    );
}

auto hash_compute_pipeline_desc(
    ComputePipelineDesc const& desc, D3D12_COMPUTE_PIPELINE_STATE_DESC const& desc_dx
) -> std::string {
    auto config_hash = hash(
        hash_bind_groups_layout(desc.bind_groups_layout),
        hash_static_samplers(desc.static_samplers),
        hash_push_constants(desc.push_constants)
    );

    auto cs_md5 = crypto::md5({reinterpret_cast<std::byte const*>(desc_dx.CS.pShaderBytecode), desc_dx.CS.BytecodeLength});

    return fmt::format(
        "COMPUTE-{:x}-{}",
        config_hash, cs_md5.as_string()
    );
}

} // namespace

auto PipelineStateCacheD3D12::init(IDXGIAdapter* adapter, ID3D12Device* device) -> void {
    adapter_ = adapter;
    device_ = device;
}

auto PipelineStateCacheD3D12::read_from(Dyn<rt::IFile>::Ref file) -> void {
    DXGI_ADAPTER_DESC adapter_desc{};
    adapter_->GetDesc(&adapter_desc);

    auto file_data = file.read_binary_data();
    ReadByteStream bs{file_data};
    
    uint32_t vendor_id = 0;
    uint32_t device_id = 0;
    uint32_t subsys_id = 0;
    uint32_t revision = 0;
    bs.read(vendor_id).read(device_id).read(subsys_id).read(revision);
    if (
        adapter_desc.VendorId != vendor_id
        || adapter_desc.DeviceId != device_id
        || adapter_desc.SubSysId != subsys_id
        || adapter_desc.Revision != revision
    ) {
        return;
    }

    size_t num_cached_pipelines = 0;
    bs.read(num_cached_pipelines);
    cached_pipeline_offsets_.reserve(num_cached_pipelines);
    for (size_t i = 0; i < num_cached_pipelines; i++) {
        std::string key;
        std::pair<size_t, size_t> value;
        bs.read(key).read(value.first).read(value.second);
        cached_pipeline_offsets_.insert({std::move(key), value});
    }

    size_t data_size = 0;
    bs.read(data_size);
    data_.resize(data_size);
    bs.read_raw(data_.data(), data_.size());
}

auto PipelineStateCacheD3D12::write_to(Dyn<rt::IFile>::Ref file) const -> void {
    DXGI_ADAPTER_DESC adapter_desc{};
    adapter_->GetDesc(&adapter_desc);

    WriteByteStream bs{};
    bs.write(adapter_desc.VendorId);
    bs.write(adapter_desc.DeviceId);
    bs.write(adapter_desc.SubSysId);
    bs.write(adapter_desc.Revision);

    bs.write(cached_pipeline_offsets_.size());
    for (auto& [key, value] : cached_pipeline_offsets_) {
        bs.write(key);
        bs.write(value.first);
        bs.write(value.second);
    }

    bs.write(data_.size()).write_raw(data_.data(), data_.size());
    bs.write(data_);

    file.write_binary_data(bs.data());
}

auto PipelineStateCacheD3D12::create_graphics_pipeline(
    GraphicsPipelineDesc const& desc,
    D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc_dx,
    Microsoft::WRL::ComPtr<ID3D12PipelineState>& pipeline
) -> HRESULT {
    std::lock_guard<std::mutex> lock{cache_mutex_};

    auto hash_key = hash_graphics_pipeline_desc(desc, desc_dx);
    auto need_to_cache = true;
    if (auto it = cached_pipeline_offsets_.find(hash_key); it != cached_pipeline_offsets_.end()) {
        desc_dx.CachedPSO.pCachedBlob = data_.data() + it->second.first;
        desc_dx.CachedPSO.CachedBlobSizeInBytes = it->second.second;
        need_to_cache = false;
    }

    auto ret = device_->CreateGraphicsPipelineState(&desc_dx, IID_PPV_ARGS(&pipeline));

    if (ret == D3D12_ERROR_ADAPTER_NOT_FOUND || ret == D3D12_ERROR_DRIVER_VERSION_MISMATCH) {
        cached_pipeline_offsets_.erase(hash_key);
        need_to_cache = true;
        desc_dx.CachedPSO = {};
        ret = device_->CreateGraphicsPipelineState(&desc_dx, IID_PPV_ARGS(&pipeline));
    }

    if (need_to_cache && ret == S_OK) {
        Microsoft::WRL::ComPtr<ID3DBlob> cache_data;
        pipeline->GetCachedBlob(&cache_data);
        auto cached_pair = std::make_pair<size_t, size_t>(data_.size(), cache_data->GetBufferSize());
        std::copy_n(
            reinterpret_cast<std::byte const*>(cache_data->GetBufferPointer()),
            cache_data->GetBufferSize(),
            std::back_inserter(data_)
        );
        cached_pipeline_offsets_.insert({std::move(hash_key), cached_pair});
    }

    return ret;
}

auto PipelineStateCacheD3D12::create_compute_pipeline(
    ComputePipelineDesc const& desc,
    D3D12_COMPUTE_PIPELINE_STATE_DESC& desc_dx,
    Microsoft::WRL::ComPtr<ID3D12PipelineState>& pipeline
) -> HRESULT {
    std::lock_guard<std::mutex> lock{cache_mutex_};

    auto hash_key = hash_compute_pipeline_desc(desc, desc_dx);
    auto need_to_cache = true;
    if (auto it = cached_pipeline_offsets_.find(hash_key); it != cached_pipeline_offsets_.end()) {
        desc_dx.CachedPSO.pCachedBlob = data_.data() + it->second.first;
        desc_dx.CachedPSO.CachedBlobSizeInBytes = it->second.second;
        need_to_cache = false;
    }

    auto ret = device_->CreateComputePipelineState(&desc_dx, IID_PPV_ARGS(&pipeline));

    if (ret == D3D12_ERROR_ADAPTER_NOT_FOUND || ret == D3D12_ERROR_DRIVER_VERSION_MISMATCH) {
        cached_pipeline_offsets_.erase(hash_key);
        need_to_cache = true;
        desc_dx.CachedPSO = {};
        ret = device_->CreateComputePipelineState(&desc_dx, IID_PPV_ARGS(&pipeline));
    }

    if (need_to_cache && ret == S_OK) {
        Microsoft::WRL::ComPtr<ID3DBlob> cache_data;
        pipeline->GetCachedBlob(&cache_data);
        auto cached_pair = std::make_pair<size_t, size_t>(data_.size(), cache_data->GetBufferSize());
        std::copy_n(
            reinterpret_cast<std::byte const*>(cache_data->GetBufferPointer()),
            cache_data->GetBufferSize(),
            std::back_inserter(data_)
        );
        cached_pipeline_offsets_.insert({std::move(hash_key), cached_pair});
    }

    return ret;
}

}
