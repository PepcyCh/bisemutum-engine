#include "device.hpp"

#include <iostream>

#ifdef interface
#undef interface
#endif
#include <fmt/format.h>
#include <bisemutum/engine/engine.hpp>
#include <bisemutum/runtime/vfs.hpp>
#include <bisemutum/runtime/logger.hpp>
#include <bisemutum/prelude/misc.hpp>
#include <bisemutum/prelude/math.hpp>

#include "volk.h"
#include "utils.hpp"
#include "command.hpp"
#include "swapchain.hpp"
#include "sync.hpp"
#include "resource.hpp"
#include "accel.hpp"
#include "sampler.hpp"
#include "descriptor.hpp"
#include "shader.hpp"
#include "pipeline.hpp"

#define BI_VULKAN_USE_DESCRIPTOR_BUFFER 0

namespace bi::rhi {

namespace {

VKAPI_ATTR auto VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    VkDebugUtilsMessengerCallbackDataEXT const* p_callback_data,
    void* p_user_data
) -> VkBool32 {
    std::cerr << "[validation layer] " << p_callback_data->pMessage << std::endl;
    return VK_FALSE;
}

auto is_device_extension_supported(CSpan<VkExtensionProperties> ext_props, char const* ext_name) -> bool {
    return std::find_if(
        ext_props.begin(), ext_props.end(),
        [ext_name](VkExtensionProperties const& prop) {
            return strcmp(prop.extensionName, ext_name) == 0;
        }
    ) != ext_props.end();
}
auto is_device_extensions_supported(CSpan<VkExtensionProperties> ext_props, CSpan<char const*> ext_names) -> bool {
    for (auto ext_name : ext_names) {
        if (!is_device_extension_supported(ext_props, ext_name)) {
            return false;
        }
    }
    return true;
}

auto to_vk_image_view_type(TextureViewType view_type) -> VkImageViewType {
    switch (view_type) {
        case TextureViewType::d1: return VK_IMAGE_VIEW_TYPE_1D;
        case TextureViewType::d1_array: return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
        case TextureViewType::d2: return VK_IMAGE_VIEW_TYPE_2D;
        case TextureViewType::d2_array: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case TextureViewType::cube: return VK_IMAGE_VIEW_TYPE_CUBE;
        case TextureViewType::cube_array: return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        case TextureViewType::d3: return VK_IMAGE_VIEW_TYPE_3D;
        default: unreachable();
    }
}

} // namespace

auto DescriptorSetLayoutHashHelper::operator()(BindGroupLayout const& v) const noexcept -> size_t {
    size_t hash = 0;
    for (auto& entry : v) {
        hash = hash_combine(hash, bi::hash(entry.binding_or_register, entry.count, entry.type, entry.visibility));
    }
    return hash;
}

auto DescriptorSetLayoutHashHelper::operator()(
    BindGroupLayout const& a, BindGroupLayout const& b
) const noexcept -> bool {
    if (a.size() == b.size()) {
        for (size_t i = 0; i < a.size(); i++) {
            if (
                a[i].binding_or_register != b[i].binding_or_register
                || a[i].count != b[i].count
                || a[i].type != b[i].type
                || a[i].visibility != b[i].visibility
            ) {
                return false;
            }
        }
        return true;
    } else {
        return false;
    }
}

auto DeviceVulkan::create(DeviceDesc const& desc) -> Box<DeviceVulkan> {
    return Box<DeviceVulkan>::make(desc);
}

DeviceVulkan::DeviceVulkan(DeviceDesc const& desc) {
    volkInitialize();

    VkApplicationInfo app_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "bisemutum",
        .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
        .pEngineName = "bisemutum",
        .engineVersion = VK_MAKE_VERSION(0, 1, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };

    VkDebugUtilsMessengerCreateInfoEXT debug_utils_ci{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext = nullptr,
        .flags = 0,
        .messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = vk_debug_callback,
        .pUserData = nullptr,
    };

    std::vector<char const*> enabled_layers{};
    if (desc.enable_validation) {
        enabled_layers.push_back("VK_LAYER_KHRONOS_validation");
    }
    std::vector<char const*> enabled_instance_extensions{
        VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef _WIN32
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#else
        VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#endif
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    };
    
    VkInstanceCreateInfo instance_ci{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = static_cast<uint32_t>(enabled_layers.size()),
        .ppEnabledLayerNames = enabled_layers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(enabled_instance_extensions.size()),
        .ppEnabledExtensionNames = enabled_instance_extensions.data(),
    };
    connect_vk_p_next_chain(&instance_ci, &debug_utils_ci);

    vkCreateInstance(&instance_ci, nullptr, &instance_);
    volkLoadInstanceOnly(instance_);

    if (desc.enable_validation) {
        vkCreateDebugUtilsMessengerEXT(instance_, &debug_utils_ci, nullptr, &debug_utils_messenger_);
    }

    pick_device(desc);

    init_allocator();

    init_descriptor_sizes();

    initialize_device_properties();
}

DeviceVulkan::~DeviceVulkan() {
    auto ret = vkDeviceWaitIdle(device_);
    BI_ASSERT_MSG(ret == VK_SUCCESS, fmt::format("Device error: {}", static_cast<uint32_t>(ret)));

    destroy_acceleration_structure_query_pools();

    save_pipeline_cache(true);

    vmaDestroyAllocator(allocator_);

    immutable_samplers_heap_.reset();
    for (auto [_, set_layout] : cached_desc_set_layouts_) {
        vkDestroyDescriptorSetLayout(device_, set_layout, nullptr);
    }
    cached_desc_set_layouts_.clear();

    vkDestroyDevice(device_, nullptr);

    if (debug_utils_messenger_ != VK_NULL_HANDLE) {
        vkDestroyDebugUtilsMessengerEXT(instance_, debug_utils_messenger_, nullptr);
    }

    vkDestroyInstance(instance_, nullptr);
}

auto DeviceVulkan::pick_device(DeviceDesc const& desc) -> void {
    uint32_t num_physical_device;
    vkEnumeratePhysicalDevices(instance_, &num_physical_device, nullptr);
    std::vector<VkPhysicalDevice> physical_devices(num_physical_device);
    vkEnumeratePhysicalDevices(instance_, &num_physical_device, physical_devices.data());

    VkPhysicalDevice available_device = VK_NULL_HANDLE;
    for (auto physical_device : physical_devices) {
        available_device = physical_device;
        VkPhysicalDeviceProperties physical_device_props;
        vkGetPhysicalDeviceProperties(physical_device, &physical_device_props);
        if (physical_device_props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            physical_device_ = physical_device;
            physical_device_props_ = physical_device_props;
        }
    }
    if (physical_device_ == VK_NULL_HANDLE) {
        if (available_device == VK_NULL_HANDLE) {
            throw std::runtime_error("no available GPU found");
        }
        physical_device_ = available_device;
        vkGetPhysicalDeviceProperties(physical_device_, &physical_device_props_);
    }

    uint32_t num_queue_family;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &num_queue_family, nullptr);
    std::vector<VkQueueFamilyProperties> queue_family_props(num_queue_family);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &num_queue_family, queue_family_props.data());

    uint32_t graphics_queue_family = ~0u;
    uint32_t transfer_queue_family = ~0u;
    uint32_t compute_queue_family = ~0u;
    for (uint32_t i = 0; i < num_queue_family; i++) {
        auto const& props = queue_family_props[i];
        if (graphics_queue_family == ~0u && (props.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            graphics_queue_family = i;
        } else if (transfer_queue_family == ~0u && (props.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0) {
            transfer_queue_family = i;
        } else if (compute_queue_family == ~0u && (props.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0) {
            compute_queue_family = i;
        }
    }
    queue_family_indices_[static_cast<uint8_t>(QueueType::graphics)] = graphics_queue_family;
    queue_family_indices_[static_cast<uint8_t>(QueueType::compute)] = compute_queue_family;
    queue_family_indices_[static_cast<uint8_t>(QueueType::transfer)] = transfer_queue_family;

    float queue_priority = 1.0f;
    std::vector<uint32_t> created_queue_family{graphics_queue_family};
    if (transfer_queue_family != ~0u) {
        created_queue_family.push_back(transfer_queue_family);
    } else {
        queue_family_indices_[static_cast<uint8_t>(QueueType::transfer)] = graphics_queue_family;
    }
    if (compute_queue_family != ~0u) {
        created_queue_family.push_back(compute_queue_family);
    } else {
        queue_family_indices_[static_cast<uint8_t>(QueueType::compute)] = graphics_queue_family;
    }
    std::vector<VkDeviceQueueCreateInfo> queue_ci(created_queue_family.size());
    for (size_t i = 0; i < queue_ci.size(); i++) {
        queue_ci[i] = VkDeviceQueueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = created_queue_family[i],
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        };
    }

    uint32_t num_supported_device_extensions;
    vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &num_supported_device_extensions, nullptr);
    std::vector<VkExtensionProperties> supported_device_extensions(num_supported_device_extensions);
    vkEnumerateDeviceExtensionProperties(
        physical_device_, nullptr, &num_supported_device_extensions, supported_device_extensions.data()
    );

    std::vector<char const*> enabled_device_extensions{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME,
    };
#if BI_VULKAN_USE_DESCRIPTOR_BUFFER
    support_descriptor_buffer_ = is_device_extension_supported(
        supported_device_extensions, VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME
    );
#endif
    if (support_descriptor_buffer_) {
        enabled_device_extensions.push_back(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME);
    }

    device_properties_.raytracing_pipeline = is_device_extensions_supported(
        supported_device_extensions, {
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_RAY_QUERY_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        }
    );
    if (device_properties_.raytracing_pipeline) {
        enabled_device_extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        enabled_device_extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
        enabled_device_extensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
        enabled_device_extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
    }

    device_properties_.meshlet_pipeline = is_device_extension_supported(
        supported_device_extensions, VK_EXT_MESH_SHADER_EXTENSION_NAME
    );
    if (device_properties_.meshlet_pipeline) {
        enabled_device_extensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
    }

    VkPhysicalDeviceFeatures2 device_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = nullptr,
    };
    VkPhysicalDeviceVulkan11Features vk11_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
    };
    connect_vk_p_next_chain(&device_features, &vk11_features);
    VkPhysicalDeviceVulkan12Features vk12_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
    };
    connect_vk_p_next_chain(&device_features, &vk12_features);
    VkPhysicalDeviceVulkan13Features vk13_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
    };
    connect_vk_p_next_chain(&device_features, &vk13_features);
    VkPhysicalDeviceDescriptorBufferFeaturesEXT desciptor_buffer_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
    };
    if (support_descriptor_buffer_) {
        connect_vk_p_next_chain(&device_features, &desciptor_buffer_features);
    }
    VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
    };
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR raytracing_pipeline_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
    };
    VkPhysicalDeviceRayQueryFeaturesKHR ray_query_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
    };
    if (device_properties_.raytracing_pipeline) {
        connect_vk_p_next_chain(&device_features, &acceleration_structure_features);
        connect_vk_p_next_chain(&device_features, &raytracing_pipeline_features);
        connect_vk_p_next_chain(&device_features, &ray_query_features);
    }
    VkPhysicalDeviceMeshShaderFeaturesEXT mesh_shader_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
    };
    if (device_properties_.meshlet_pipeline) {
        connect_vk_p_next_chain(&device_features, &mesh_shader_features);
    }

    vkGetPhysicalDeviceFeatures2(physical_device_, &device_features);

    VkDeviceCreateInfo device_ci{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueCreateInfoCount = static_cast<uint32_t>(queue_ci.size()),
        .pQueueCreateInfos = queue_ci.data(),
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<uint32_t>(enabled_device_extensions.size()),
        .ppEnabledExtensionNames = enabled_device_extensions.data(),
        .pEnabledFeatures = nullptr,
    };
    connect_vk_p_next_chain(&device_ci, &device_features);

    vkCreateDevice(physical_device_, &device_ci, nullptr, &device_);
    volkLoadDevice(device_);

    for (size_t i = 0; i < 3; i++) {
        if (queue_family_indices_[i] != ~0u) {
            queues_[i] = Box<QueueVulkan>::make(unsafe_make_ref(this), queue_family_indices_[i]);
        }
    }
}

auto DeviceVulkan::init_allocator() -> void {
    VmaVulkanFunctions vk_functions{
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
    };

    VmaAllocatorCreateInfo allocator_ci{};
    allocator_ci.vulkanApiVersion = VK_API_VERSION_1_3;
    allocator_ci.instance = instance_;
    allocator_ci.physicalDevice = physical_device_;
    allocator_ci.device = device_;
    allocator_ci.pVulkanFunctions = &vk_functions;
    allocator_ci.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocator_ci, &allocator_);
}

auto DeviceVulkan::initialize_device_properties() -> void {
    device_properties_.gpu_name = physical_device_props_.deviceName;
    device_properties_.max_num_bind_groups = physical_device_props_.limits.maxBoundDescriptorSets;
    device_properties_.separate_sampler_heap = support_descriptor_buffer_;
    device_properties_.descriptor_heap_suballocation = support_descriptor_buffer_;

    if (device_properties_.raytracing_pipeline) {
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_pipeline_props{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
            .pNext = nullptr,
        };
        VkPhysicalDeviceProperties2 physical_device_properties{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &rt_pipeline_props,
        };
        vkGetPhysicalDeviceProperties2(physical_device_, &physical_device_properties);
        rt_sbt_requirements_.handle_size = rt_pipeline_props.shaderGroupHandleSize;
        rt_sbt_requirements_.handle_alignment = rt_pipeline_props.shaderGroupHandleAlignment;
        rt_sbt_requirements_.base_alignment = rt_pipeline_props.shaderGroupBaseAlignment;
    }
}

auto DeviceVulkan::init_descriptor_sizes() -> void {
    if (!support_descriptor_buffer_) { return; }

    VkPhysicalDeviceDescriptorBufferPropertiesEXT desc_buffer_props{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT,
        .pNext = nullptr,
    };
    VkPhysicalDeviceProperties2 device_props{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &desc_buffer_props,
    };
    vkGetPhysicalDeviceProperties2(physical_device_, &device_props);

    descriptor_offset_alignment_ = desc_buffer_props.descriptorBufferOffsetAlignment;

    descriptor_sizes_[static_cast<size_t>(DescriptorType::sampler)] = desc_buffer_props.samplerDescriptorSize;
    descriptor_sizes_[static_cast<size_t>(DescriptorType::uniform_buffer)] =
        desc_buffer_props.uniformBufferDescriptorSize;
    descriptor_sizes_[static_cast<size_t>(DescriptorType::read_only_storage_buffer)] =
        desc_buffer_props.storageBufferDescriptorSize;
    descriptor_sizes_[static_cast<size_t>(DescriptorType::read_write_storage_buffer)] =
        desc_buffer_props.storageBufferDescriptorSize;
    descriptor_sizes_[static_cast<size_t>(DescriptorType::sampled_texture)] =
        desc_buffer_props.sampledImageDescriptorSize;
    descriptor_sizes_[static_cast<size_t>(DescriptorType::read_only_storage_texture)] =
        desc_buffer_props.storageImageDescriptorSize;
    descriptor_sizes_[static_cast<size_t>(DescriptorType::read_write_storage_texture)] =
        desc_buffer_props.storageImageDescriptorSize;
    descriptor_sizes_[static_cast<size_t>(DescriptorType::acceleration_structure)] =
        desc_buffer_props.accelerationStructureDescriptorSize;

    descriptor_sizes_[static_cast<size_t>(DescriptorType::none)] = std::max({
        desc_buffer_props.samplerDescriptorSize,
        desc_buffer_props.uniformBufferDescriptorSize,
        desc_buffer_props.storageBufferDescriptorSize,
        desc_buffer_props.sampledImageDescriptorSize,
        desc_buffer_props.storageImageDescriptorSize,
        desc_buffer_props.accelerationStructureDescriptorSize,
    });
}

auto DeviceVulkan::save_pipeline_cache(bool destroy) -> void {
    if (pipeline_cache_ && !pipeline_cache_file_path_.empty()) {
        size_t pipeline_cache_size = 0;
        vkGetPipelineCacheData(device_, pipeline_cache_, &pipeline_cache_size, nullptr);
        std::vector<std::byte> pipeline_cache_data(pipeline_cache_size);
        vkGetPipelineCacheData(device_, pipeline_cache_, &pipeline_cache_size, pipeline_cache_data.data());
        g_engine->file_system()->create_file(pipeline_cache_file_path_).value().write_binary_data(pipeline_cache_data);
        if (destroy) {
            vkDestroyPipelineCache(device_, pipeline_cache_, nullptr);
            pipeline_cache_ = VK_NULL_HANDLE;
        }
    }
}

auto DeviceVulkan::require_descriptor_set_layout(BindGroupLayout const& layout) -> VkDescriptorSetLayout {
    if (auto it = cached_desc_set_layouts_.find(layout); it != cached_desc_set_layouts_.end()) {
        return it->second;
    }

    std::vector<VkDescriptorSetLayoutBinding> bindings_info(layout.size());
    for (size_t i = 0; auto const &entry : layout) {
        bindings_info[i].binding = entry.binding_or_register;
        bindings_info[i].descriptorCount = entry.count;
        bindings_info[i].descriptorType = to_vk_descriptor_type(entry.type);
        bindings_info[i].pImmutableSamplers = nullptr;
        bindings_info[i].stageFlags = to_vk_shader_stages(entry.visibility);
        ++i;
    }
    VkDescriptorSetLayoutCreateFlags flags = support_descriptor_buffer_
        ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        : 0u;
    VkDescriptorSetLayoutCreateInfo set_layout_ci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags,
        .bindingCount = static_cast<uint32_t>(bindings_info.size()),
        .pBindings = bindings_info.data(),
    };
    VkDescriptorSetLayout set_layout;
    vkCreateDescriptorSetLayout(device_, &set_layout_ci, nullptr, &set_layout);
    cached_desc_set_layouts_.insert({layout, set_layout});
    return set_layout;
}

auto DeviceVulkan::immutable_samplers_heap() -> Ptr<DescriptorHeapVulkanLegacy> {
    if (!support_descriptor_buffer_ && !immutable_samplers_heap_) {
        DescriptorHeapDesc heap_desc{
            .max_count = 2048,
            .type = DescriptorHeapType::sampler,
            .shader_visible = true,
        };
        immutable_samplers_heap_ = Box<DescriptorHeapVulkanLegacy>::make(unsafe_make_ref(this), heap_desc);
    }
    return immutable_samplers_heap_.get();
}

auto DeviceVulkan::raytracing_shader_binding_table_requirements() const -> RaytracingShaderBindingTableRequirements {
    return rt_sbt_requirements_;
}

auto DeviceVulkan::get_queue(QueueType type) -> Ref<Queue> {
    return queues_[static_cast<size_t>(type)].ref();
}

auto DeviceVulkan::create_command_pool(CommandPoolDesc const& desc) -> Box<CommandPool> {
    return Box<CommandPoolVulkan>::make(unsafe_make_ref(this), desc);
}

auto DeviceVulkan::create_swapchain(const SwapchainDesc &desc) -> Box<Swapchain> {
    return Box<SwapchainVulkan>::make(unsafe_make_ref(this), desc);
}

auto DeviceVulkan::create_fence() -> Box<Fence> {
    return Box<FenceVulkan>::make(unsafe_make_ref(this));
}

auto DeviceVulkan::create_semaphore() -> Box<Semaphore> {
    return Box<SemaphoreVulkan>::make(unsafe_make_ref(this));
}

auto DeviceVulkan::create_buffer(BufferDesc const& desc) -> Box<Buffer> {
    return Box<BufferVulkan>::make(unsafe_make_ref(this), desc);
}

auto DeviceVulkan::create_texture(TextureDesc const& desc) -> Box<Texture> {
    return Box<TextureVulkan>::make(unsafe_make_ref(this), desc);
}

auto DeviceVulkan::create_sampler(SamplerDesc const& desc) -> Box<Sampler> {
    return Box<SamplerVulkan>::make(unsafe_make_ref(this), desc);
}

auto DeviceVulkan::create_acceleration_structure(AccelerationStructureDesc const& desc) -> Box<AccelerationStructure> {
    return Box<AccelerationStructureVulkan>::make(unsafe_make_ref(this), desc);
}

auto DeviceVulkan::create_descriptor_heap(DescriptorHeapDesc const& desc) -> Box<DescriptorHeap> {
    if (support_descriptor_buffer_) {
        return Box<DescriptorHeapVulkan>::make(unsafe_make_ref(this), desc);
    } else {
        return Box<DescriptorHeapVulkanLegacy>::make(unsafe_make_ref(this), desc);
    }
}

auto DeviceVulkan::create_shader_module(ShaderModuleDesc const& desc) -> Box<ShaderModule> {
    return Box<ShaderModuleVulkan>::make(unsafe_make_ref(this), desc);
}

auto DeviceVulkan::create_graphics_pipeline(GraphicsPipelineDesc const& desc) -> Box<GraphicsPipeline> {
    return Box<GraphicsPipelineVulkan>::make(unsafe_make_ref(this), desc);
}

auto DeviceVulkan::create_compute_pipeline(ComputePipelineDesc const& desc) -> Box<ComputePipeline> {
    return Box<ComputePipelineVulkan>::make(unsafe_make_ref(this), desc);
}

auto DeviceVulkan::create_raytracing_pipeline(RaytracingPipelineDesc const& desc) -> Box<RaytracingPipeline> {
    return Box<RaytracingPipelineVulkan>::make(unsafe_make_ref(this), desc);
}

auto DeviceVulkan::create_descriptor(BufferDescriptorDesc const& buffer_desc, DescriptorHandle handle) -> void {
    auto buffer_vk = buffer_desc.buffer.cast_to<BufferVulkan>();
    auto specified_size = buffer_desc.size;
    if (specified_size != static_cast<uint64_t>(-1) && buffer_desc.structure_stride > 0) {
        specified_size *= buffer_desc.structure_stride;
    }
    auto available_size = std::min(buffer_vk->desc().size - buffer_desc.offset, specified_size);
    if (support_descriptor_buffer_) {
        VkDescriptorGetInfoEXT get_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
            .pNext = nullptr,
        };
        VkDescriptorAddressInfoEXT addr_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
            .pNext = nullptr,
            .address = buffer_vk->address() + buffer_desc.offset,
            .range = available_size,
        };
        switch (buffer_desc.type) {
            case DescriptorType::uniform_buffer:
                get_info.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                get_info.data.pUniformBuffer = &addr_info;
                break;
            case DescriptorType::read_only_storage_buffer:
            case DescriptorType::read_write_storage_buffer:
                get_info.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                get_info.data.pStorageBuffer = &addr_info;
                break;
            default: unreachable();
        }
        vkGetDescriptorEXT(
            device_, &get_info, size_of_descriptor(buffer_desc.type), reinterpret_cast<void*>(handle.cpu)
        );
    } else {
        auto desc_set = *reinterpret_cast<VkDescriptorSet*>(handle.cpu);
        VkDescriptorBufferInfo buffer_info{
            .buffer = buffer_vk->raw(),
            .offset = buffer_desc.offset,
            .range = available_size,
        };
        VkWriteDescriptorSet desc_write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = desc_set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = to_vk_descriptor_type(buffer_desc.type),
            .pImageInfo = nullptr,
            .pBufferInfo = &buffer_info,
            .pTexelBufferView = nullptr,
        };
        vkUpdateDescriptorSets(device_, 1, &desc_write, 0, nullptr);
    }
}

auto DeviceVulkan::create_descriptor(TextureDescriptorDesc const& texture_desc, DescriptorHandle handle) -> void {
    auto texture_vk = texture_desc.texture.cast_to<TextureVulkan>();
    auto format = texture_desc.format == ResourceFormat::undefined ? texture_vk->desc().format : texture_desc.format;
    bool is_depth_stencil = is_depth_stencil_format(format);
    auto view_type = texture_desc.view_type == TextureViewType::automatic
        ? texture_vk->get_automatic_view_type()
        : texture_desc.view_type;
    VkDescriptorImageInfo image_info{
        .sampler = VK_NULL_HANDLE,
        .imageView = texture_vk->get_view(
            texture_desc.base_level, texture_desc.num_levels,
            texture_desc.base_layer, texture_desc.num_layers,
            to_vk_format(format), to_vk_image_view_type(view_type), true
        ),
    };
    switch (texture_desc.type) {
        case DescriptorType::sampled_texture:
            image_info.imageLayout = is_depth_stencil
                ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            break;
        case DescriptorType::read_only_storage_texture:
        case DescriptorType::read_write_storage_texture:
            image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            break;
        default: unreachable();
    }
    if (support_descriptor_buffer_) {
        VkDescriptorGetInfoEXT get_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
            .pNext = nullptr,
        };
        switch (texture_desc.type) {
            case DescriptorType::sampled_texture:
                get_info.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                get_info.data.pSampledImage = &image_info;
                break;
            case DescriptorType::read_only_storage_texture:
            case DescriptorType::read_write_storage_texture:
                get_info.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                get_info.data.pStorageImage = &image_info;
                break;
            default: unreachable();
        }
        vkGetDescriptorEXT(
            device_, &get_info, size_of_descriptor(texture_desc.type), reinterpret_cast<void*>(handle.cpu)
        );
    } else {
        auto desc_set = *reinterpret_cast<VkDescriptorSet*>(handle.cpu);
        VkWriteDescriptorSet desc_write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = desc_set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = to_vk_descriptor_type(texture_desc.type),
            .pImageInfo = &image_info,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr,
        };
        vkUpdateDescriptorSets(device_, 1, &desc_write, 0, nullptr);
    }
}

auto DeviceVulkan::create_descriptor(Ref<Sampler> sampler, DescriptorHandle handle) -> void {
    auto sampler_vk = sampler.cast_to<SamplerVulkan>();
    auto raw_sampler = sampler_vk->raw();
    if (support_descriptor_buffer_) {
        VkDescriptorGetInfoEXT get_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
            .pNext = nullptr,
            .type = VK_DESCRIPTOR_TYPE_SAMPLER,
        };
        get_info.data.pSampler = &raw_sampler;
        vkGetDescriptorEXT(
            device_, &get_info, size_of_descriptor(DescriptorType::sampler), reinterpret_cast<void*>(handle.cpu)
        );
    } else {
        auto desc_set = *reinterpret_cast<VkDescriptorSet*>(handle.cpu);
        VkDescriptorImageInfo sampler_info{
            .sampler = raw_sampler,
            .imageView = VK_NULL_HANDLE,
        };
        VkWriteDescriptorSet desc_write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = desc_set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .pImageInfo = &sampler_info,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr,
        };
        vkUpdateDescriptorSets(device_, 1, &desc_write, 0, nullptr);
    }
}

auto DeviceVulkan::create_descriptor(Ref<AccelerationStructure> accel, DescriptorHandle handle) -> void {
    auto accel_vk = accel.cast_to<AccelerationStructureVulkan>();
    if (support_descriptor_buffer_) {
        VkDescriptorGetInfoEXT get_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
            .pNext = nullptr,
            .type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .data = {
                .accelerationStructure = accel_vk->gpu_reference(),
            },
        };
        vkGetDescriptorEXT(
            device_, &get_info, size_of_descriptor(DescriptorType::acceleration_structure),
            reinterpret_cast<void*>(handle.cpu)
        );
    } else {
        auto desc_set = *reinterpret_cast<VkDescriptorSet*>(handle.cpu);
        auto accel_raw = accel_vk->raw();
        VkWriteDescriptorSetAccelerationStructureKHR desc_accel_write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .pNext = nullptr,
            .accelerationStructureCount = 1,
            .pAccelerationStructures = &accel_raw,
        };
        VkWriteDescriptorSet desc_write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = &desc_accel_write,
            .dstSet = desc_set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .pImageInfo = nullptr,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr,
        };
        vkUpdateDescriptorSets(device_, 1, &desc_write, 0, nullptr);
    }
}

auto DeviceVulkan::copy_descriptors(
    DescriptorHandle dst_desciptor,
    CSpan<DescriptorHandle> src_descriptors,
    BindGroupLayout const& bind_group_layout
) -> void {
    if (support_descriptor_buffer_) {
        auto vk_layout = require_descriptor_set_layout(bind_group_layout);
        auto p_dst = reinterpret_cast<std::byte*>(dst_desciptor.cpu);
        size_t index = 0;
        for (auto& entry : bind_group_layout) {
            auto size = size_of_descriptor(entry.type);
            VkDeviceSize offset;
            vkGetDescriptorSetLayoutBindingOffsetEXT(device_, vk_layout, entry.binding_or_register, &offset);
            for (uint32_t i = 0; i < entry.count; i++) {
                std::memcpy(p_dst + offset, reinterpret_cast<void const*>(src_descriptors[index].cpu), size);
                offset += size;
                ++index;
            }
        }
    } else {
        auto dst_desc_set = *reinterpret_cast<VkDescriptorSet*>(dst_desciptor.cpu);
        std::vector<VkCopyDescriptorSet> desc_copies(src_descriptors.size());
        size_t index = 0;
        for (auto& entry : bind_group_layout) {
            for (uint32_t i = 0; i < entry.count; i++) {
                auto src_desc_set = *reinterpret_cast<VkDescriptorSet*>(src_descriptors[index].cpu);
                desc_copies[index] = VkCopyDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .srcSet = src_desc_set,
                    .srcBinding = 0,
                    .srcArrayElement = 0,
                    .dstSet = dst_desc_set,
                    .dstBinding = entry.binding_or_register,
                    .dstArrayElement = i,
                    .descriptorCount = 1,
                };
                ++index;
            }
        }
        vkUpdateDescriptorSets(device_, 0, nullptr, desc_copies.size(), desc_copies.data());
    }
}

auto DeviceVulkan::initialize_pipeline_cache_from(std::string_view cache_file_path) -> void {
    std::vector<std::byte> pipeline_cache_data{};

    pipeline_cache_file_path_ = cache_file_path;
    auto cache_file_opt = g_engine->file_system()->get_file(cache_file_path);
    if (cache_file_opt) {
        pipeline_cache_data = cache_file_opt.value().read_binary_data();
    }

    VkPipelineCacheCreateInfo pipeline_cache_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .initialDataSize = pipeline_cache_data.size(),
        .pInitialData = pipeline_cache_data.data(),
    };
    vkCreatePipelineCache(device_, &pipeline_cache_ci, nullptr, &pipeline_cache_);
}

auto DeviceVulkan::get_acceleration_structure_memory_size(
    AccelerationStructureGeometryBuildInput const& build_info
) -> AccelerationStructureMemoryInfo {
    VkAccelerationStructureBuildGeometryInfoKHR vk_build_info;
    std::vector<VkAccelerationStructureGeometryKHR> vk_geometries;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> vk_range_infos;
    to_vk_accel_build_info(build_info, vk_build_info, vk_geometries, vk_range_infos);
    std::vector<uint32_t> primitive_counts(vk_range_infos.size());
    for (size_t i = 0; i < vk_range_infos.size(); i++) {
        primitive_counts[i] = vk_range_infos[i].primitiveCount;
    }

    VkAccelerationStructureBuildSizesInfoKHR size_info{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
        .pNext = nullptr,
    };
    vkGetAccelerationStructureBuildSizesKHR(
        device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &vk_build_info, primitive_counts.data(), &size_info
    );
    return AccelerationStructureMemoryInfo{
        .acceleration_structure_size = size_info.accelerationStructureSize,
        .build_scratch_size = size_info.buildScratchSize,
        .update_scratch_size = size_info.updateScratchSize,
    };
}

auto DeviceVulkan::get_acceleration_structure_memory_size(
    AccelerationStructureInstanceBuildInput const& build_info
) -> AccelerationStructureMemoryInfo {
    VkAccelerationStructureBuildGeometryInfoKHR vk_build_info;
    VkAccelerationStructureGeometryKHR vk_geometry;
    VkAccelerationStructureBuildRangeInfoKHR vk_range_info;
    to_vk_accel_build_info(build_info, vk_build_info, vk_geometry, vk_range_info);

    VkAccelerationStructureBuildSizesInfoKHR size_info{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
        .pNext = nullptr,
    };
    vkGetAccelerationStructureBuildSizesKHR(
        device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &vk_build_info, &vk_range_info.primitiveCount, &size_info
    );
    return AccelerationStructureMemoryInfo{
        .acceleration_structure_size = size_info.accelerationStructureSize,
        .build_scratch_size = size_info.buildScratchSize,
        .update_scratch_size = size_info.updateScratchSize,
    };
}

auto DeviceVulkan::require_acceleration_structure_query_pools(
    AccelerationStructureQueryPoolSizes const& sizes
) -> AccelerationStructureQueryPools const& {
    auto& pools = accel_query_pools_.emplace_back();

    VkQueryPoolCreateInfo query_pool_ci{
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pipelineStatistics = 0,
    };

    query_pool_ci.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
    query_pool_ci.queryCount = sizes.num_compacted_size;
    if (query_pool_ci.queryCount > 0) {
        vkCreateQueryPool(device_, &query_pool_ci, nullptr, &pools.compacted_size);
    }

    return pools;
}

auto DeviceVulkan::destroy_acceleration_structure_query_pools() -> void {
    for (auto& pools : accel_query_pools_) {
        if (pools.compacted_size) {
            vkDestroyQueryPool(device_, pools.compacted_size, nullptr);
        }
    }
    accel_query_pools_.clear();
}

}
