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
#include "sampler.hpp"
#include "descriptor.hpp"
#include "shader.hpp"
#include "pipeline.hpp"

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
}

DeviceVulkan::~DeviceVulkan() {
    auto ret = vkDeviceWaitIdle(device_);
    BI_ASSERT_MSG(ret == VK_SUCCESS, fmt::format("Device error: {}", static_cast<uint32_t>(ret)));

    save_pipeline_cache(true);

    vmaDestroyAllocator(allocator_);

    vkDestroyDevice(device_, nullptr);

    if (debug_utils_messenger_ != VK_NULL_HANDLE) {
        vkDestroyDebugUtilsMessengerEXT(instance_, debug_utils_messenger_, nullptr);
    }

    vkDestroyInstance(instance_, nullptr);
}

auto DeviceVulkan::initialize_device_properties() -> void {
    device_properties_.gpu_name = physical_device_props_.deviceName;
    device_properties_.max_num_bind_groups = physical_device_props_.limits.maxBoundDescriptorSets;
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
        const auto &props = queue_family_props[i];
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

    std::vector<char const*> enabled_device_extensions{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
    };

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
    connect_vk_p_next_chain(&device_features, &desciptor_buffer_features);

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
        // .vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
        // .vkAllocateMemory = vkAllocateMemory,
        // .vkFreeMemory = vkFreeMemory,
        // .vkMapMemory = vkMapMemory,
        // .vkUnmapMemory = vkUnmapMemory,
        // .vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
        // .vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
        // .vkBindBufferMemory = vkBindBufferMemory,
        // .vkBindImageMemory = vkBindImageMemory,
        // .vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
        // .vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
        // .vkCreateBuffer = vkCreateBuffer,
        // .vkDestroyBuffer = vkDestroyBuffer,
        // .vkCreateImage = vkCreateImage,
        // .vkDestroyImage = vkDestroyImage,
        // .vkCmdCopyBuffer = vkCmdCopyBuffer,
        // .vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2KHR,
        // .vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2KHR,
        // .vkBindBufferMemory2KHR = vkBindBufferMemory2KHR,
        // .vkBindImageMemory2KHR = vkBindImageMemory2KHR,
        // .vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2KHR,
        // .vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements,
        // .vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements,
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

auto DeviceVulkan::init_descriptor_sizes() -> void {
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

auto DeviceVulkan::create_descriptor_heap(DescriptorHeapDesc const& desc) -> Box<DescriptorHeap> {
    return Box<DescriptorHeapVulkan>::make(unsafe_make_ref(this), desc);
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

auto DeviceVulkan::create_descriptor(BufferDescriptorDesc const& buffer_desc, DescriptorHandle handle) -> void {
    auto buffer_vk = buffer_desc.buffer.cast_to<BufferVulkan>();
    auto specified_size = buffer_desc.size;
    if (specified_size != static_cast<uint64_t>(-1) && buffer_desc.structure_stride > 0) {
        specified_size *= buffer_desc.structure_stride;
    }
    auto available_size = std::min(buffer_vk->desc().size - buffer_desc.offset, specified_size);
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
}

auto DeviceVulkan::create_descriptor(TextureDescriptorDesc const& texture_desc, DescriptorHandle handle) -> void {
    auto texture_vk = texture_desc.texture.cast_to<TextureVulkan>();
    auto format = texture_desc.format == ResourceFormat::undefined ? texture_vk->desc().format : texture_desc.format;
    auto view_type = texture_desc.view_type == TextureViewType::automatic
        ? texture_vk->get_automatic_view_type()
        : texture_desc.view_type;
    VkDescriptorGetInfoEXT get_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
        .pNext = nullptr,
    };
    VkDescriptorImageInfo image_info{
        .imageView = texture_vk->get_view(
            texture_desc.base_level, texture_desc.num_levels,
            texture_desc.base_layer, texture_desc.num_layers,
            to_vk_format(format), to_vk_image_view_type(view_type)
        ),
    };
    switch (texture_desc.type) {
        case DescriptorType::sampled_texture:
            get_info.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            get_info.data.pSampledImage = &image_info;
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            break;
        case DescriptorType::read_only_storage_texture:
        case DescriptorType::read_write_storage_texture:
            get_info.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            get_info.data.pStorageImage = &image_info;
            image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            break;
        default: unreachable();
    }
    vkGetDescriptorEXT(
        device_, &get_info, size_of_descriptor(texture_desc.type), reinterpret_cast<void*>(handle.cpu)
    );
}

auto DeviceVulkan::create_descriptor(Ref<Sampler> sampler, DescriptorHandle handle) -> void {
    auto sampler_vk = sampler.cast_to<SamplerVulkan>();
    auto raw_sampler = sampler_vk->raw();
    VkDescriptorGetInfoEXT get_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
        .pNext = nullptr,
        .type = VK_DESCRIPTOR_TYPE_SAMPLER,
    };
    get_info.data.pSampler = &raw_sampler;
    vkGetDescriptorEXT(
        device_, &get_info, size_of_descriptor(DescriptorType::sampler), reinterpret_cast<void*>(handle.cpu)
    );
}

auto DeviceVulkan::copy_descriptors(
    DescriptorHandle dst_desciptor,
    CSpan<DescriptorHandle> src_descriptors,
    CSpan<DescriptorType> src_descriptors_type
) -> void {
    auto p_dst = reinterpret_cast<std::byte*>(dst_desciptor.cpu);
    size_t offset = 0;
    for (size_t i = 0; i < src_descriptors.size(); i++) {
        auto size = size_of_descriptor(src_descriptors_type[i]);
        offset = aligned_size<size_t>(offset, size);
        std::memcpy(p_dst + offset, reinterpret_cast<void const*>(src_descriptors[i].cpu), size);
        offset += size;
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

}