#include "device.hpp"

#ifdef interface
#undef interface
#endif
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/vfs.hpp>
#include <bisemutum/runtime/logger.hpp>
#include <bisemutum/prelude/misc.hpp>
#include <bisemutum/prelude/math.hpp>

#include "command.hpp"
#include "queue.hpp"
#include "sampler.hpp"
#include "shader.hpp"
#include "swapchain.hpp"
#include "sync.hpp"
#include "resource.hpp"
#include "accel.hpp"
#include "pipeline.hpp"
#include "utils.hpp"

namespace bi::rhi {

namespace {

constexpr auto make_d3d12_feature_level(uint32_t major, uint32_t minor) -> D3D_FEATURE_LEVEL {
    return static_cast<D3D_FEATURE_LEVEL>((major << 12) | (minor << 8));
}

auto wchars_to_string(wchar_t const* wchars) -> std::string {
    auto size = WideCharToMultiByte(CP_UTF8, 0, wchars, -1, nullptr, 0, nullptr, nullptr);
    auto temp  = new char[size];
    WideCharToMultiByte(CP_UTF8, 0, wchars, -1, temp, size, nullptr, nullptr);
    std::string str(temp, temp + size - 1);
    delete[] temp;
    return str;
}

}

Box<DeviceD3D12> DeviceD3D12::create(DeviceDesc const& desc) {
    return Box<DeviceD3D12>::make(desc);
}

DeviceD3D12::DeviceD3D12(DeviceDesc const& desc) {
    if (desc.enable_validation) {
        D3D12GetDebugInterface(IID_PPV_ARGS(&debug_));
        debug_->EnableDebugLayer();
    }

#ifndef NDEBUG
    UINT factory_flags = DXGI_CREATE_FACTORY_DEBUG;
#else
    UINT factory_flags = 0;
#endif
    CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&factory_));

    std::vector<IDXGIAdapter3*> adapters;
    {
        UINT i = 0;
        IDXGIAdapter3 *p;
        while (
            factory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&p))
            != DXGI_ERROR_NOT_FOUND
        ) {
            adapters.push_back(p);
            ++i;
        }
    }
    constexpr D3D_FEATURE_LEVEL d3d12_feature_level = make_d3d12_feature_level(12, 1);
    IDXGIAdapter3* selected_adapter = nullptr;
    DXGI_ADAPTER_DESC1 adapter_desc{};
    for (auto adapter : adapters) {
        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);
        if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) {
            continue;
        }
        auto ret = D3D12CreateDevice(adapter, d3d12_feature_level, IID_PPV_ARGS(reinterpret_cast<ID3D12Device**>(0)));
        if (FAILED(ret)) {
            continue;
        }

        selected_adapter = adapter;
        adapter_desc = desc;
        break;
    }

    D3D12CreateDevice(selected_adapter, d3d12_feature_level, IID_PPV_ARGS(&device_));
    adapter_ = selected_adapter;

    if (desc.enable_validation) {
        device_->QueryInterface(IID_PPV_ARGS(&info_queue_));
        info_queue_->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        info_queue_->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);

        D3D12_MESSAGE_ID denied_id[]{
            // see https://stackoverflow.com/questions/69805245/directx-12-application-is-crashing-in-windows-11
            D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
            D3D12_MESSAGE_ID_EMPTY_ROOT_DESCRIPTOR_TABLE,
        };
        D3D12_INFO_QUEUE_FILTER filter{
            .DenyList = D3D12_INFO_QUEUE_FILTER_DESC{
                .NumIDs = _countof(denied_id),
                .pIDList = denied_id,
            }
        };
        info_queue_->AddStorageFilterEntries(&filter);
    }

    initialize_device_properties();

    create_queues();

    D3D12MA::ALLOCATOR_DESC allocator_desc{
        .Flags = D3D12MA::ALLOCATOR_FLAG_NONE,
        .pDevice = device_.Get(),
        .pAdapter = selected_adapter,
    };
    D3D12MA::CreateAllocator(&allocator_desc, &allocator_);

    create_cpu_descriptor_heaps();
}

DeviceD3D12::~DeviceD3D12() {
    for (size_t i = 0; i < queues_.size(); i++) {
        queues_[i]->wait_idle();
    }
    save_pso_cache();
    allocator_->Release();
    allocator_ = nullptr;
}

auto DeviceD3D12::initialize_device_properties() -> void {
    DXGI_ADAPTER_DESC adapter_desc{};
    adapter_->GetDesc(&adapter_desc);
    device_properties_.gpu_name = wchars_to_string(adapter_desc.Description);
    // Maximum size of root signature is 64 DWORDS and each descriptor table costs 1 DWORD
    device_properties_.max_num_bind_groups = 64;

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 feature_supports5{};
    device_->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &feature_supports5, sizeof(feature_supports5));
    device_properties_.raytracing_pipeline = feature_supports5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;

    D3D12_FEATURE_DATA_D3D12_OPTIONS7 feature_supports7{};
    device_->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &feature_supports7, sizeof(feature_supports7));
    device_properties_.meshlet_pipeline = feature_supports7.MeshShaderTier != D3D12_MESH_SHADER_TIER_NOT_SUPPORTED;
}

auto DeviceD3D12::create_queues() -> void {
    std::array<D3D12_COMMAND_LIST_TYPE, 3> types{
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        D3D12_COMMAND_LIST_TYPE_COMPUTE,
        D3D12_COMMAND_LIST_TYPE_COPY,
    };
    for (size_t i = 0; i < 3; i++) {
        queues_[i] = Box<QueueD3D12>::make(unsafe_make_ref(this), types[i]);
    }
}

auto DeviceD3D12::create_cpu_descriptor_heaps() -> void {
    auto ref_this = unsafe_make_ref(this);
    rtv_heap_ = Box<RenderTargetDescriptorHeapD3D12>::make(ref_this, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    dsv_heap_ = Box<RenderTargetDescriptorHeapD3D12>::make(ref_this, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

auto DeviceD3D12::initialize_pipeline_cache_from(std::string_view cache_file_path) -> void {
    pipeline_cahce_.init(adapter_, device_.Get());

    pso_cache_file_path_ = cache_file_path;
    auto cache_file_opt = g_engine->file_system()->get_file(cache_file_path);
    if (cache_file_opt) {
        pipeline_cahce_.read_from(*&cache_file_opt.value());
    }
}

auto DeviceD3D12::save_pso_cache() -> void {
    if (!pso_cache_file_path_.empty()) {
        auto cache_file = g_engine->file_system()->create_file(pso_cache_file_path_).value();
        pipeline_cahce_.write_to(*&cache_file);
    }
}

auto DeviceD3D12::raytracing_shader_binding_table_requirements() const -> RaytracingShaderBindingTableRequirements {
    return RaytracingShaderBindingTableRequirements{
        .handle_size = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES,
        .handle_alignment = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT,
        .base_alignment = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT,
    };
}

auto DeviceD3D12::get_queue(QueueType type) -> Ref<Queue> {
    return queues_[static_cast<size_t>(type)].ref();
}

auto DeviceD3D12::create_command_pool(CommandPoolDesc const& desc) -> Box<CommandPool> {
    return Box<CommandPoolD3D12>::make(unsafe_make_ref(this), desc);
}

auto DeviceD3D12::create_swapchain(SwapchainDesc const& desc) -> Box<Swapchain> {
    return Box<SwapchainD3D12>::make(unsafe_make_ref(this), desc);
}

auto DeviceD3D12::create_fence() -> Box<Fence> {
    return Box<FenceD3D12>::make(unsafe_make_ref(this));
}

auto DeviceD3D12::create_semaphore() -> Box<Semaphore> {
    return Box<SemaphoreD3D12>::make();
}

auto DeviceD3D12::create_buffer(BufferDesc const& desc) -> Box<Buffer> {
    return Box<BufferD3D12>::make(unsafe_make_ref(this), desc);
}

auto DeviceD3D12::create_texture(TextureDesc const& desc) -> Box<Texture> {
    return Box<TextureD3D12>::make(unsafe_make_ref(this), desc);
}

auto DeviceD3D12::create_sampler(SamplerDesc const& desc) -> Box<Sampler> {
    return Box<SamplerD3D12>::make(unsafe_make_ref(this), desc);
}

auto DeviceD3D12::create_acceleration_structure(AccelerationStructureDesc const& desc) -> Box<AccelerationStructure> {
    return Box<AccelerationStructureD3D12>::make(unsafe_make_ref(this), desc);
}

auto DeviceD3D12::create_descriptor_heap(DescriptorHeapDesc const& desc) -> Box<DescriptorHeap> {
    return Box<DescriptorHeapD3D12>::make(unsafe_make_ref(this), desc);
}

auto DeviceD3D12::create_shader_module(ShaderModuleDesc const& desc) -> Box<ShaderModule> {
    return Box<ShaderModuleD3D12>::make(unsafe_make_ref(this), desc);
}

auto DeviceD3D12::create_graphics_pipeline(GraphicsPipelineDesc const& desc) -> Box<GraphicsPipeline> {
    return Box<GraphicsPipelineD3D12>::make(unsafe_make_ref(this), desc);
}

auto DeviceD3D12::create_compute_pipeline(ComputePipelineDesc const& desc) -> Box<ComputePipeline> {
    return Box<ComputePipelineD3D12>::make(unsafe_make_ref(this), desc);
}

auto DeviceD3D12::create_raytracing_pipeline(RaytracingPipelineDesc const& desc) -> Box<RaytracingPipeline> {
    return Box<RaytracingPipelineD3D12>::make(unsafe_make_ref(this), desc);
}

auto DeviceD3D12::create_descriptor(BufferDescriptorDesc const& buffer_desc, DescriptorHandle handle) -> void {
    auto buffer_dx = buffer_desc.buffer.cast_to<BufferD3D12>();
    auto specified_size = buffer_desc.size;
    if (specified_size != static_cast<uint64_t>(-1) && buffer_desc.structure_stride > 0) {
        specified_size *= buffer_desc.structure_stride;
    }
    auto available_size = std::min<uint32_t>(buffer_dx->size() - buffer_desc.offset, specified_size);
    switch (buffer_desc.type) {
        case DescriptorType::uniform_buffer: {
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc{
                .BufferLocation = buffer_dx->raw()->GetGPUVirtualAddress() + buffer_desc.offset,
                .SizeInBytes = available_size,
            };
            device_->CreateConstantBufferView(&cbv_desc, {handle.cpu});
            break;
        }
        case DescriptorType::read_only_storage_buffer: {
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{
                .Format = DXGI_FORMAT_UNKNOWN,
                .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
                .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                .Buffer = D3D12_BUFFER_SRV{
                    .FirstElement = buffer_desc.offset,
                    .NumElements = available_size / std::max(1u, buffer_desc.structure_stride),
                    .StructureByteStride = std::max(1u, buffer_desc.structure_stride),
                    .Flags = buffer_desc.structure_stride == 0 ? D3D12_BUFFER_SRV_FLAG_RAW : D3D12_BUFFER_SRV_FLAG_NONE,
                },
            };
            device_->CreateShaderResourceView(buffer_dx->raw(), &srv_desc, {handle.cpu});
            break;
        }
        case DescriptorType::read_write_storage_buffer: {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{
                .Format = DXGI_FORMAT_UNKNOWN,
                .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
                .Buffer = D3D12_BUFFER_UAV{
                    .FirstElement = buffer_desc.offset,
                    .NumElements = available_size / std::max(1u, buffer_desc.structure_stride),
                    .StructureByteStride = std::max(1u, buffer_desc.structure_stride),
                    .CounterOffsetInBytes = 0,
                    .Flags = buffer_desc.structure_stride == 0 ? D3D12_BUFFER_UAV_FLAG_RAW : D3D12_BUFFER_UAV_FLAG_NONE,
                },
            };
            device_->CreateUnorderedAccessView(buffer_dx->raw(), nullptr, &uav_desc, {handle.cpu});
            break;
        }
        default: unreachable();
    }
}

auto DeviceD3D12::create_descriptor(TextureDescriptorDesc const& texture_desc, DescriptorHandle handle) -> void {
    auto texture_dx = texture_desc.texture.cast_to<TextureD3D12>();
    auto format = texture_desc.format == ResourceFormat::undefined ? texture_dx->desc().format : texture_desc.format;
    auto view_type = texture_desc.view_type == TextureViewType::automatic
        ? texture_dx->get_automatic_view_type()
        : texture_desc.view_type;
    switch (texture_desc.type) {
        case DescriptorType::sampled_texture:
        case DescriptorType::read_only_storage_texture: {
            D3D12_SHADER_RESOURCE_VIEW_DESC src_desc{
                .Format = to_dx_format(format),
                .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            };
            if (is_depth_stencil_format(format)) {
                switch (format) {
                    case ResourceFormat::d16_unorm:
                        src_desc.Format = DXGI_FORMAT_R16_UNORM;
                        break;
                    case ResourceFormat::d24_unorm_s8_uint:
                        src_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
                        break;
                    case ResourceFormat::d32_sfloat:
                        src_desc.Format = DXGI_FORMAT_R32_FLOAT;
                        break;
                    case ResourceFormat::d32_sfloat_s8_uint:
                        src_desc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
                        break;
                    default:
                        log::warn("general", "D3D12 unexpected SRV format for depth texture");
                        break;
                }
            }
            switch (view_type) {
                case TextureViewType::d1:
                    src_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
                    src_desc.Texture1D.MostDetailedMip = texture_desc.base_level;
                    src_desc.Texture1D.MipLevels = texture_desc.num_levels;
                    src_desc.Texture1D.ResourceMinLODClamp = 0.0f;
                    break;
                case TextureViewType::d1_array:
                    src_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
                    src_desc.Texture1DArray.MostDetailedMip = texture_desc.base_level;
                    src_desc.Texture1DArray.MipLevels = texture_desc.num_levels;
                    src_desc.Texture1DArray.ResourceMinLODClamp = 0.0f;
                    src_desc.Texture1DArray.FirstArraySlice = texture_desc.base_layer;
                    src_desc.Texture1DArray.ArraySize = texture_desc.num_layers;
                    break;
                case TextureViewType::d2:
                    src_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                    src_desc.Texture2D.MostDetailedMip = texture_desc.base_level;
                    src_desc.Texture2D.MipLevels = texture_desc.num_levels;
                    src_desc.Texture2D.ResourceMinLODClamp = 0.0f;
                    src_desc.Texture2D.PlaneSlice = 0;
                    break;
                case TextureViewType::d2_array:
                    src_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                    src_desc.Texture2DArray.MostDetailedMip = texture_desc.base_level;
                    src_desc.Texture2DArray.MipLevels = texture_desc.num_levels;
                    src_desc.Texture2DArray.ResourceMinLODClamp = 0.0f;
                    src_desc.Texture2DArray.PlaneSlice = 0;
                    src_desc.Texture2DArray.FirstArraySlice = texture_desc.base_layer;
                    src_desc.Texture2DArray.ArraySize = texture_desc.num_layers;
                    break;
                case TextureViewType::cube:
                    src_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                    src_desc.TextureCube.MostDetailedMip = texture_desc.base_level;
                    src_desc.TextureCube.MipLevels = texture_desc.num_levels;
                    src_desc.TextureCube.ResourceMinLODClamp = 0.0f;
                    break;
                case TextureViewType::cube_array:
                    src_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
                    src_desc.TextureCubeArray.MostDetailedMip = texture_desc.base_level;
                    src_desc.TextureCubeArray.MipLevels = texture_desc.num_levels;
                    src_desc.TextureCubeArray.ResourceMinLODClamp = 0.0f;
                    src_desc.TextureCubeArray.First2DArrayFace = texture_desc.base_layer;
                    src_desc.TextureCubeArray.NumCubes =
                        texture_desc.num_layers == ~0u ? ~0u : texture_desc.num_layers / 6;
                    break;
                case TextureViewType::d3:
                    src_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
                    src_desc.Texture3D.MostDetailedMip = texture_desc.base_level;
                    src_desc.Texture3D.MipLevels = texture_desc.num_levels;
                    src_desc.Texture3D.ResourceMinLODClamp = 0.0f;
                    break;
                default: unreachable();
            }
            device_->CreateShaderResourceView(texture_dx->raw(), &src_desc, {handle.cpu});
            break;
        }
        case DescriptorType::read_write_storage_texture: {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{
                .Format = to_dx_format(format),
            };
            switch (view_type) {
                case TextureViewType::d1:
                    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
                    uav_desc.Texture1D.MipSlice = texture_desc.base_level;
                    break;
                case TextureViewType::d1_array:
                    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
                    uav_desc.Texture1DArray.MipSlice = texture_desc.base_level;
                    uav_desc.Texture1DArray.FirstArraySlice = texture_desc.base_layer;
                    uav_desc.Texture1DArray.ArraySize = texture_desc.num_layers;
                    break;
                case TextureViewType::d2:
                    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                    uav_desc.Texture2D.MipSlice = texture_desc.base_level;
                    uav_desc.Texture2D.PlaneSlice = 0;
                    break;
                case TextureViewType::d2_array:
                case TextureViewType::cube:
                case TextureViewType::cube_array:
                    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                    uav_desc.Texture2DArray.MipSlice = texture_desc.base_level;
                    uav_desc.Texture2DArray.PlaneSlice = 0;
                    uav_desc.Texture2DArray.FirstArraySlice = texture_desc.base_layer;
                    uav_desc.Texture2DArray.ArraySize = texture_desc.num_layers;
                    break;
                case TextureViewType::d3:
                    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
                    uav_desc.Texture3D.MipSlice = texture_desc.base_level;
                    uav_desc.Texture3D.FirstWSlice = texture_desc.base_layer;
                    uav_desc.Texture3D.WSize = texture_desc.num_layers;
                    break;
                default: unreachable();
            }
            device_->CreateUnorderedAccessView(texture_dx->raw(), nullptr, &uav_desc, {handle.cpu});
            break;
        }
        default: unreachable();
    }
}

auto DeviceD3D12::create_descriptor(Ref<Sampler> sampler, DescriptorHandle handle) -> void {
    auto sampler_dx = sampler.cast_to<SamplerD3D12>();
    device_->CreateSampler(&sampler_dx->sampler_desc(), {handle.cpu});
}

auto DeviceD3D12::create_descriptor(Ref<AccelerationStructure> accel, DescriptorHandle handle) -> void {
    auto accel_dx = accel.cast_to<AccelerationStructureD3D12>();
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{
        .Format = DXGI_FORMAT_UNKNOWN,
        .ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .RaytracingAccelerationStructure = {
            .Location = accel_dx->gpu_reference(),
        },
    };
    device_->CreateShaderResourceView(nullptr, &srv_desc, {handle.cpu});
}

auto DeviceD3D12::copy_descriptors(
    DescriptorHandle dst_desciptor,
    CSpan<DescriptorHandle> src_descriptors,
    BindGroupLayout const& bind_group_layout
) -> void {
    if (bind_group_layout.empty()) { return; }
    auto heap_type = bind_group_layout[0].type == DescriptorType::sampler
        ? D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
        : D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    auto stride = device_->GetDescriptorHandleIncrementSize(heap_type);
    for (size_t i = 0; i < src_descriptors.size(); i++) {
        device_->CopyDescriptorsSimple(1, {dst_desciptor.cpu + stride * i}, {src_descriptors[i].cpu}, heap_type);
    }
}

auto DeviceD3D12::get_acceleration_structure_memory_size(
    AccelerationStructureGeometryBuildInput const& build_info
) -> AccelerationStructureMemoryInfo {
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS dx_build_info;
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> dx_geometries;
    to_dx_accel_build_input(build_info, dx_build_info, dx_geometries);

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info{};
    device_->GetRaytracingAccelerationStructurePrebuildInfo(&dx_build_info, &prebuild_info);
    return AccelerationStructureMemoryInfo{
        .acceleration_structure_size = aligned_size<uint32_t>(
            prebuild_info.ResultDataMaxSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT
        ),
        .build_scratch_size = aligned_size<uint32_t>(
            prebuild_info.ScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT
        ),
        .update_scratch_size = aligned_size<uint32_t>(
            prebuild_info.UpdateScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT
        ),
    };
}

auto DeviceD3D12::get_acceleration_structure_memory_size(
    AccelerationStructureInstanceBuildInput const& build_info
) -> AccelerationStructureMemoryInfo {
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS dx_build_info;
    to_dx_accel_build_input(build_info, dx_build_info);

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info{};
    device_->GetRaytracingAccelerationStructurePrebuildInfo(&dx_build_info, &prebuild_info);
    return AccelerationStructureMemoryInfo{
        .acceleration_structure_size = prebuild_info.ResultDataMaxSizeInBytes,
        .build_scratch_size = prebuild_info.ScratchDataSizeInBytes,
        .update_scratch_size = prebuild_info.UpdateScratchDataSizeInBytes,
    };
}

auto DeviceD3D12::get_local_root_signature(uint32_t size_in_bytes, uint32_t space, uint32_t register_) -> ID3D12RootSignature* {
    size_in_bytes = aligned_size<uint32_t>(size_in_bytes, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
    auto key = std::make_tuple(size_in_bytes, space, register_);
    auto [it, need_to_create] = caced_local_root_signatures_.try_emplace(key);
    if (need_to_create) {
        D3D12_ROOT_PARAMETER1 root_param{
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
            .Constants = D3D12_ROOT_CONSTANTS{
                .ShaderRegister = register_,
                .RegisterSpace = space,
                .Num32BitValues = (size_in_bytes + 3) / 4,
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        };
        D3D12_VERSIONED_ROOT_SIGNATURE_DESC signature_desc{
            .Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
            .Desc_1_1 = D3D12_ROOT_SIGNATURE_DESC1 {
                .NumParameters = 1,
                .pParameters = &root_param,
                .NumStaticSamplers = 0,
                .pStaticSamplers = nullptr,
                .Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE,
            },
        };
        Microsoft::WRL::ComPtr<ID3DBlob> serialized_signature;
        Microsoft::WRL::ComPtr<ID3DBlob> error;
        D3D12SerializeVersionedRootSignature(&signature_desc, &serialized_signature, &error);
        device_->CreateRootSignature(
            0, serialized_signature->GetBufferPointer(), serialized_signature->GetBufferSize(),
            IID_PPV_ARGS(&it->second)
        );
    }
    return it->second.Get();
}

}
