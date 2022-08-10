#include "device.hpp"

#include <vector>
#include <iostream>

#include <GLFW/glfw3.h>

#include "core/logger.hpp"
#include "utils.hpp"
#include "swap_chain.hpp"
#include "sync.hpp"
#include "resource.hpp"
#include "sampler.hpp"
#include "shader.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

namespace {

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
    void* p_user_data) {
    std::cerr << "[validation layer] " << p_callback_data->pMessage << std::endl;
    return VK_FALSE;
}

}

Ptr<DeviceVulkan> DeviceVulkan::Create(const DeviceDesc &desc) {
    return Ptr<DeviceVulkan>::Make(desc);
}

DeviceVulkan::DeviceVulkan(const DeviceDesc &desc) {
    volkInitialize();

    VkApplicationInfo app_info {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "Bismuth",
        .applicationVersion = VK_MAKE_VERSION(BISMUTH_VERSION_MAJOR, BISMUTH_VERSION_MINOR, BISMUTH_VERSION_PATCH),
        .pEngineName = "Bismuth",
        .engineVersion = VK_MAKE_VERSION(BISMUTH_VERSION_MAJOR, BISMUTH_VERSION_MINOR, BISMUTH_VERSION_PATCH),
        .apiVersion = VK_MAKE_API_VERSION(0, BISMUTH_VULKAN_VERSION_MAJOR, BISMUTH_VULKAN_VERSION_MINOR, 0),
    };

    VkDebugUtilsMessengerCreateInfoEXT debug_utils_ci {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext = nullptr,
        .flags = 0,
        .messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = DebugCallback,
        .pUserData = nullptr,
    };

    Vec<const char *> enabled_layers = {};
    if (desc.enable_validation) {
        enabled_layers.push_back("VK_LAYER_KHRONOS_validation");
    }
    Vec<const char *> enabled_instance_extensions = {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    };
    uint32_t num_glfw_extensions;
    const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&num_glfw_extensions);
    for (uint32_t i = 0; i < num_glfw_extensions; i++) {
        enabled_instance_extensions.push_back(glfw_extensions[i]);
    }
    
    VkInstanceCreateInfo instance_ci {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = static_cast<uint32_t>(enabled_layers.size()),
        .ppEnabledLayerNames = enabled_layers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(enabled_instance_extensions.size()),
        .ppEnabledExtensionNames = enabled_instance_extensions.data(),
    };
    ConnectVkPNextChain(&instance_ci, &debug_utils_ci);

    vkCreateInstance(&instance_ci, nullptr, &instance_);
    volkLoadInstance(instance_);

    if (desc.enable_validation) {
        vkCreateDebugUtilsMessengerEXT(instance_, &debug_utils_ci, nullptr, &debug_utils_messenger_);
    }

    glfwCreateWindowSurface(instance_, desc.window, nullptr, &surface_);
    surface_format_ = ToVkFormat(desc.surface_format);

    PickDevice(desc);

    InitializeAllocator();
}

DeviceVulkan::~DeviceVulkan() {
    vmaDestroyAllocator(allocator_);

    vkDestroyDevice(device_, nullptr);

    vkDestroySurfaceKHR(instance_, surface_, nullptr);
    if (debug_utils_messenger_ != VK_NULL_HANDLE) {
        vkDestroyDebugUtilsMessengerEXT(instance_, debug_utils_messenger_, nullptr);
    }

    vkDestroyInstance(instance_, nullptr);
}

void DeviceVulkan::PickDevice(const DeviceDesc &desc) {
    uint32_t num_physical_device;
    vkEnumeratePhysicalDevices(instance_, &num_physical_device, nullptr);
    Vec<VkPhysicalDevice> physical_devices(num_physical_device);
    vkEnumeratePhysicalDevices(instance_, &num_physical_device, physical_devices.data());

    VkPhysicalDevice available_device = VK_NULL_HANDLE;
    for (VkPhysicalDevice physical_device : physical_devices) {
        uint32_t num_surface_format;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface_, &num_surface_format, nullptr);
        if (num_surface_format == 0) {
            continue;
        }
        Vec<VkSurfaceFormatKHR> surface_formats(num_surface_format);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface_, &num_surface_format, surface_formats.data());
        bool surface_format_ok = std::find_if(surface_formats.begin(), surface_formats.end(),
            [&](const VkSurfaceFormatKHR &surface_format) {
                return surface_format.format == surface_format_
                    && surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            }) != surface_formats.end();
        if (!surface_format_ok) {
            continue;
        }
        
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
            BI_CRTICAL(gGraphicsLogger, "No available GPU found");
        }
        physical_device_ = available_device;
        vkGetPhysicalDeviceProperties(physical_device_, &physical_device_props_);
    }
    BI_INFO(gGraphicsLogger, "Use GPU: {}", physical_device_props_.deviceName);

    uint32_t num_queue_family;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &num_queue_family, nullptr);
    Vec<VkQueueFamilyProperties> queue_family_props(num_queue_family);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &num_queue_family, queue_family_props.data());
    
    uint32_t graphics_queue_family = ~0u;
    uint32_t transfer_queue_family = ~0u;
    uint32_t compute_queue_family = ~0u;
    for (uint32_t i = 0; i < num_queue_family; i++) {
        const auto &props = queue_family_props[i];
        if ((props.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            graphics_queue_family = i;
        } else if ((props.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0) {
            transfer_queue_family = i;
        } else if ((props.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0) {
            compute_queue_family = i;
        }
    }
    queue_family_indices[static_cast<uint8_t>(QueueType::eGraphics)] = graphics_queue_family;
    queue_family_indices[static_cast<uint8_t>(QueueType::eCompute)] = compute_queue_family;
    queue_family_indices[static_cast<uint8_t>(QueueType::eTransfer)] = transfer_queue_family;

    float queue_priority = 1.0f;
    Vec<uint32_t> created_queue_family = { graphics_queue_family };
    if (transfer_queue_family != ~0u) {
        created_queue_family.push_back(transfer_queue_family);
    }
    if (compute_queue_family != ~0u) {
        created_queue_family.push_back(compute_queue_family);
    }
    Vec<VkDeviceQueueCreateInfo> queue_ci(created_queue_family.size());
    for (size_t i = 0; i < queue_ci.size(); i++) {
        queue_ci[i] = VkDeviceQueueCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = created_queue_family[i],
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        };
    }

    Vec<const char *> enabled_device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME,
#if BISMUTH_VULKAN_VERSION_MINOR < 3
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
#endif
    };

    VkPhysicalDeviceFeatures2 device_features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = nullptr,
        .features = VkPhysicalDeviceFeatures {},
    };
#if BISMUTH_VULKAN_VERSION_MINOR >= 1
    VkPhysicalDeviceVulkan11Features vk11_features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES
    };
    ConnectVkPNextChain(&device_features, &vk11_features);
#endif
#if BISMUTH_VULKAN_VERSION_MINOR >= 2
    VkPhysicalDeviceVulkan12Features vk12_features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES
    };
    ConnectVkPNextChain(&device_features, &vk12_features);
#endif
#if BISMUTH_VULKAN_VERSION_MINOR >= 3
    VkPhysicalDeviceVulkan13Features vk13_features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES
    };
    ConnectVkPNextChain(&device_features, &vk13_features);
#endif
    VkPhysicalDeviceConservativeRasterizationPropertiesEXT conservative_rasterization_features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONSERVATIVE_RASTERIZATION_PROPERTIES_EXT,
    };
    ConnectVkPNextChain(&device_features, &conservative_rasterization_features);
#if BISMUTH_VULKAN_VERSION_MINOR < 3
    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
    };
    ConnectVkPNextChain(&device_features, &dynamic_rendering_features);
#endif
    vkGetPhysicalDeviceFeatures2(physical_device_, &device_features);

    VkDeviceCreateInfo device_ci {
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
    ConnectVkPNextChain(&device_ci, &device_features);

    vkCreateDevice(physical_device_, &device_ci, nullptr, &device_);
    volkLoadDevice(device_);
}

void DeviceVulkan::InitializeAllocator() {
    VmaVulkanFunctions vk_functions {
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

    VmaAllocatorCreateInfo allocator_ci {};
    allocator_ci.vulkanApiVersion =
        VK_MAKE_API_VERSION(0, BISMUTH_VULKAN_VERSION_MAJOR, BISMUTH_VULKAN_VERSION_MINOR, 0);
    allocator_ci.instance = instance_;
    allocator_ci.physicalDevice = physical_device_;
    allocator_ci.device = device_;
    allocator_ci.pVulkanFunctions = &vk_functions;
    allocator_ci.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocator_ci, &allocator_);
}

Ptr<Queue> DeviceVulkan::GetQueue(QueueType type) {
    return Ptr<QueueVulkan>::Make(RefThis(), queue_family_indices[static_cast<uint8_t>(type)]);
}

Ptr<SwapChain> DeviceVulkan::CreateSwapChain(Ref<Queue> queue, uint32_t width, uint32_t height) {
    return Ptr<SwapChainVulkan>::Make(RefThis(), queue.CastTo<QueueVulkan>(), width, height);
}

Ptr<Fence> DeviceVulkan::CreateFence() {
    return Ptr<FenceVulkan>::Make(RefThis());
}

Ptr<Semaphore> DeviceVulkan::CreateSemaphore() {
    return Ptr<SemaphoreVulkan>::Make(RefThis());
}

Ptr<Buffer> DeviceVulkan::CreateBuffer(const BufferDesc &desc) {
    return Ptr<BufferVulkan>::Make(RefThis(), desc);
}

Ptr<Texture> DeviceVulkan::CreateTexture(const TextureDesc &desc) {
    return Ptr<TextureVulkan>::Make(RefThis(), desc);
}

Ptr<Sampler> DeviceVulkan::CreateSampler(const SamplerDesc &desc) {
    return Ptr<SamplerVulkan>::Make(RefThis(), desc);
}

Ptr<ShaderModule> DeviceVulkan::CreateShaderModule(const Vec<uint8_t> &src_bytes) {
    return Ptr<ShaderModuleVulkan>::Make(RefThis(), src_bytes);
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
