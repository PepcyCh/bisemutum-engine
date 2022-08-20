#include "descriptor.hpp"

#include "device.hpp"
#include "resource.hpp"
#include "sampler.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

namespace {

DescriptorHandle GetBufferDescriptorHandle(const BufferRange &buffer, const DescriptorSetLayoutBinding &binding) {
    auto buffer_dx = buffer.buffer.CastTo<BufferD3D12>();
    return buffer_dx->GetView(BufferViewD3D12Desc {
        .offset = buffer.offset,
        .size = buffer.length,
        .descriptor_type = binding.type,
        .struct_stride = binding.struct_stride,
    });
}

DescriptorHandle GetTextureDescriptorHandle(const TextureView &texture, const DescriptorSetLayoutBinding &binding) {
    auto texture_dx = texture.texture.CastTo<TextureD3D12>();
    return texture_dx->GetView(TextureViewD3D12Desc {
        .base_layer = texture.base_layer,
        .layers = texture.layers,
        .base_level = texture.base_level,
        .levels = texture.levels,
        .descriptor_type = binding.type,
        .dim = binding.tex_dim,
        .format = binding.tex_format == ResourceFormat::eUndefined
            ? texture_dx->RawFormat() : ToDxFormat(binding.tex_format),
    });
}

}

DescriptorHeapD3D12::DescriptorHeapD3D12(Ref<class DeviceD3D12> device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT max_count,
    bool shader_visible)
    : device_(device), type_(type), shader_visible_(shader_visible), max_count_(max_count), used_count_(0) {
    D3D12_DESCRIPTOR_HEAP_DESC desc {
        .Type = type,
        .NumDescriptors = max_count,
        .Flags = shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0,
    };
    device->Raw()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap_));

    descriptor_size_ = device->Raw()->GetDescriptorHandleIncrementSize(type);

    start_handle_ = DescriptorHandle {
        .cpu = heap_->GetCPUDescriptorHandleForHeapStart(),
        .gpu = { .ptr = 0 },
    };
    if (shader_visible) {
        start_handle_.gpu = heap_->GetGPUDescriptorHandleForHeapStart();
    }
}

DescriptorHeapD3D12::~DescriptorHeapD3D12() {}

DescriptorHandle DescriptorHeapD3D12::AllocateDecriptor() {
    BI_ASSERT(used_count_ < max_count_);

    DescriptorHandle handle {
        .cpu = { .ptr = start_handle_.cpu.ptr + used_count_ * descriptor_size_ },
        .gpu = { .ptr = 0 },
    };
    if (shader_visible_) {
        handle.gpu.ptr = start_handle_.gpu.ptr + used_count_ * descriptor_size_ ;
    }
    ++used_count_;

    return handle;
}

DescriptorHandle ShaderVisibleDescriptorHeapD3D12::AllocateAndWriteDescriptors(const DescriptorSetLayout &layout,
    const ShaderParams &values) {
    uint32_t num_descriptors = 0;
    for (const auto &binding : layout.bindings) {
        num_descriptors += binding.count;
    }
    BI_ASSERT(used_count_ + num_descriptors <= max_count_);

    DescriptorHandle handle {
        .cpu = { .ptr = start_handle_.cpu.ptr + used_count_ * descriptor_size_ },
        .gpu = { .ptr = start_handle_.gpu.ptr + used_count_ * descriptor_size_ },
    };
    used_count_ += num_descriptors;

    DescriptorHandle p_handle = handle;
    for (uint32_t binding = 0; binding < values.resources.size(); binding++) {
        const auto &resource = values.resources[binding];
        const auto &binding_info = layout.bindings[binding];
        resource.Match(
            [](std::monostate) {},
            [this, &binding_info, &p_handle](const BufferRange &buffer) {
                auto view_handle = GetBufferDescriptorHandle(buffer, binding_info);
                device_->Raw()->CopyDescriptorsSimple(1, p_handle.cpu, view_handle.cpu,
                    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                p_handle.cpu.ptr += descriptor_size_;
                p_handle.gpu.ptr += descriptor_size_;
            },
            [this, &binding_info, &p_handle](const TextureView &texture) {
                auto view_handle = GetTextureDescriptorHandle(texture, binding_info);
                device_->Raw()->CopyDescriptorsSimple(1, p_handle.cpu, view_handle.cpu,
                    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                p_handle.cpu.ptr += descriptor_size_;
                p_handle.gpu.ptr += descriptor_size_;
            },
            [this, &binding_info, &p_handle](Ref<Sampler> sampler) {
                auto sampler_dx = sampler.CastTo<SamplerD3D12>();
                device_->Raw()->CopyDescriptorsSimple(1, p_handle.cpu, sampler_dx->GetDescriptorHandle().cpu,
                    D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
                p_handle.cpu.ptr += descriptor_size_;
                p_handle.gpu.ptr += descriptor_size_;
            },
            [this, &binding_info, &p_handle](const Vec<BufferRange> &buffers) {
                for (const auto &buffer : buffers) {
                    auto view_handle = GetBufferDescriptorHandle(buffer, binding_info);
                    device_->Raw()->CopyDescriptorsSimple(1, p_handle.cpu, view_handle.cpu,
                        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                    p_handle.cpu.ptr += descriptor_size_;
                    p_handle.gpu.ptr += descriptor_size_;
                }
            },
            [this, &binding_info, &p_handle](const Vec<TextureView> &textures) {
                for (const auto &texture : textures) {
                    auto view_handle = GetTextureDescriptorHandle(texture, binding_info);
                    device_->Raw()->CopyDescriptorsSimple(1, p_handle.cpu, view_handle.cpu,
                        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                    p_handle.cpu.ptr += descriptor_size_;
                    p_handle.gpu.ptr += descriptor_size_;
                }
            },
            [this, &binding_info, &p_handle](const Vec<Ref<Sampler>> &samplers) {
                for (const auto &sampler : samplers) {
                    auto sampler_dx = sampler.CastTo<SamplerD3D12>();
                    device_->Raw()->CopyDescriptorsSimple(1, p_handle.cpu, sampler_dx->GetDescriptorHandle().cpu,
                        D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
                    p_handle.cpu.ptr += descriptor_size_;
                    p_handle.gpu.ptr += descriptor_size_;
                }
            }
        );
    }

    return handle;
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
