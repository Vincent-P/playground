#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VMA_IMPLEMENTATION
#define THSVS_SIMPLER_VULKAN_SYNCHRONIZATION_IMPLEMENTATION

#include "renderer/vlk_context.hpp"
#include "window.hpp"

#include <iostream>
#include <thsvs/thsvs_simpler_vulkan_synchronization.h>
#include <vector>
#include <vk_mem_alloc.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace my_app::vulkan
{

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                                                     VkDebugUtilsMessageTypeFlagsEXT message_type,
                                                     const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                     void * /*unused*/)
{
    std::string severity{};
    switch (message_severity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        severity = "[INFO]";
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        severity = "[ERROR]";
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        severity = "[VERBOSE]";
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        severity = "[WARNING]";
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT:
        break;
    }

    std::string type{};
    if (message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
        type += "[GENERAL]";
    }
    if (message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
        type += "[VALIDATION]";
    }
    if (message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
        return VK_FALSE;
    }

    std::cerr << severity << type << " " << pCallbackData->pMessage << "\n";

    if (pCallbackData->objectCount) {
        std::cerr << "Objects: \n";
        for (size_t i = 0; i < pCallbackData->objectCount; i++) {
            auto &object = pCallbackData->pObjects[i];
            std::cerr << "\t [" << i << "] " << vk::to_string(static_cast<vk::ObjectType>(object.objectType)) << " "
                      << (object.pObjectName ? object.pObjectName : "NoName") << std::endl;
        }
    }

    return VK_FALSE;
}

Context Context::create(const Window &window)
{
    Context ctx;

    // the default dispatcher needs to be initialized with this first so that enumarateInstanceLayerProperties work
    vk::DynamicLoader dl;
    auto vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    /// --- Create Instance
    std::vector<const char *> instance_extensions;

    uint32_t required_count;
    const char **required_extensions = glfwGetRequiredInstanceExtensions(&required_count);
    for (size_t i = 0; i < required_count; i++) {
        instance_extensions.push_back(required_extensions[i]);
    }

    if (ENABLE_VALIDATION_LAYERS) {
        instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    auto installed_instance_layers = vk::enumerateInstanceLayerProperties();
    std::vector<const char *> instance_layers;
    if (ENABLE_VALIDATION_LAYERS) {
        instance_layers.push_back("VK_LAYER_LUNARG_standard_validation");
    }

    vk::ApplicationInfo app_info;
    app_info.pApplicationName   = "Test Vulkan";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName        = "GoodEngine";
    app_info.engineVersion      = VK_MAKE_VERSION(1, 1, 0);
    app_info.apiVersion         = VK_API_VERSION_1_2;

    vk::InstanceCreateInfo create_info;
    create_info.flags                   = {};
    create_info.pApplicationInfo        = &app_info;
    create_info.enabledLayerCount       = static_cast<uint32_t>(instance_layers.size());
    create_info.ppEnabledLayerNames     = instance_layers.data();
    create_info.enabledExtensionCount   = static_cast<uint32_t>(instance_extensions.size());
    create_info.ppEnabledExtensionNames = instance_extensions.data();

    ctx.instance = vk::createInstanceUnique(create_info);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*ctx.instance);

    /// --- Init debug layers
    if (ENABLE_VALIDATION_LAYERS) {
        vk::DebugUtilsMessengerCreateInfoEXT ci;
        ci.flags           = {};
        ci.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                             | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                             | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        ci.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                         | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
                         | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
        ci.pfnUserCallback = debug_callback;

        ctx.debug_messenger = ctx.instance->createDebugUtilsMessengerEXT(ci, nullptr);
    }

    /// --- Create the surface
    VkSurfaceKHR surfaceTmp;
    glfwCreateWindowSurface(static_cast<VkInstance>(*ctx.instance), window.get_handle(), nullptr, &surfaceTmp);
    ctx.surface = vk::UniqueSurfaceKHR(surfaceTmp, *ctx.instance);

    /// --- Pick a physical device
    auto physical_devices = ctx.instance->enumeratePhysicalDevices();
    ctx.physical_device   = physical_devices[0];
    for (const auto &d : physical_devices) {
        auto properties = d.getProperties();
        std::cout << "Device: " << properties.deviceName << "\n";
        if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
            std::cout << "Selected: " << properties.deviceName << "\n";
            ctx.physical_device = d;
        }
    }

    /// --- Create the logical device
    auto installed_extensions = ctx.physical_device.enumerateDeviceExtensionProperties();
    std::vector<const char *> device_extensions;
    device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    std::vector<const char *> device_layers;

    ctx.physical_device_features = vk::PhysicalDeviceFeatures2{};
    ctx.vulkan12_features = vk::PhysicalDeviceVulkan12Features{};
    ctx.physical_device_features.pNext = &ctx.vulkan12_features;

    ctx.physical_device.getFeatures2(&ctx.physical_device_features);

    auto queue_families = ctx.physical_device.getQueueFamilyProperties();

    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
    float priority = 0.0;

    bool has_graphics = false;
    bool has_present  = false;
    for (uint32_t i = 0; i < queue_families.size(); i++) {
        if (!has_graphics && queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics) {
            // Create a single graphics queue.
            queue_create_infos.emplace_back(vk::DeviceQueueCreateFlags(), i, 1, &priority);
            has_graphics            = true;
            ctx.graphics_family_idx = i;
        }

        if (!has_present && ctx.physical_device.getSurfaceSupportKHR(i, *ctx.surface)) {
            // Create a single graphics queue.
            queue_create_infos.emplace_back(vk::DeviceQueueCreateFlags(), i, 1, &priority);
            has_present            = true;
            ctx.present_family_idx = i;
        }
    }

    if (!has_present || !has_graphics) {
        throw std::runtime_error("failed to find a graphics and present queue.");
    }

    if (ctx.present_family_idx == ctx.graphics_family_idx) {
        queue_create_infos.pop_back();
    }

    vk::DeviceCreateInfo dci;
    dci.pNext                   = &ctx.physical_device_features;
    dci.flags                   = {};
    dci.queueCreateInfoCount    = static_cast<uint32_t>(queue_create_infos.size());
    dci.pQueueCreateInfos       = queue_create_infos.data();
    dci.enabledLayerCount       = static_cast<uint32_t>(device_layers.size());
    dci.ppEnabledLayerNames     = device_layers.data();
    dci.enabledExtensionCount   = static_cast<uint32_t>(device_extensions.size());
    dci.ppEnabledExtensionNames = device_extensions.data();
    dci.pEnabledFeatures        = nullptr;

    ctx.device = ctx.physical_device.createDeviceUnique(dci);

    ctx.physical_props = ctx.physical_device.getProperties();

    /// --- Init VMA allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice         = ctx.physical_device;
    allocatorInfo.device                 = *ctx.device;
    vmaCreateAllocator(&allocatorInfo, &ctx.allocator);

    /// --- Create the swapchain
    ctx.create_swapchain();

    ctx.create_frame_resources();

    /// --- The descriptor sets of the pool are recycled manually
    std::array pool_sizes{
        vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 1024),
    };

    vk::DescriptorPoolCreateInfo dpci{};
    dpci.flags = {/*vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind*/};
    dpci.poolSizeCount  = pool_sizes.size();
    dpci.pPoolSizes     = pool_sizes.data();
    dpci.maxSets        = 512;
    ctx.descriptor_pool = ctx.device->createDescriptorPoolUnique(dpci);

    return ctx;
}

void Context::create_swapchain()
{
    auto &ctx = *this;

    // Use default extent for the swapchain
    auto capabilities    = ctx.physical_device.getSurfaceCapabilitiesKHR(*ctx.surface);
    ctx.swapchain.extent = capabilities.currentExtent;

#if 0
        std::cout << "Create SwapChain with extent: " << ctx.swapchain.extent.width << "x" << ctx.swapchain.extent.height << "\n";
#endif

    // Find a good present mode (by priority Mailbox then Immediate then FIFO)
    auto present_modes         = ctx.physical_device.getSurfacePresentModesKHR(*ctx.surface);
    ctx.swapchain.present_mode = vk::PresentModeKHR::eFifo;

    for (auto &pm : present_modes) {
        if (pm == vk::PresentModeKHR::eMailbox) {
            ctx.swapchain.present_mode = vk::PresentModeKHR::eMailbox;
            break;
        }
    }

    if (ctx.swapchain.present_mode == vk::PresentModeKHR::eFifo) {
        for (auto &pm : present_modes) {
            if (pm == vk::PresentModeKHR::eImmediate) {
                ctx.swapchain.present_mode = vk::PresentModeKHR::eImmediate;
                break;
            }
        }
    }

    // Find the best format
    auto formats         = ctx.physical_device.getSurfaceFormatsKHR(*ctx.surface);
    ctx.swapchain.format = formats[0];
    if (ctx.swapchain.format.format == vk::Format::eUndefined) {
        ctx.swapchain.format.format     = vk::Format::eB8G8R8A8Unorm;
        ctx.swapchain.format.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
    }
    else {
        for (const auto &f : formats) {
            if (f.format == vk::Format::eB8G8R8A8Unorm && f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                ctx.swapchain.format = f;
                break;
            }
        }
    }

    vk::SwapchainCreateInfoKHR ci{};
    ci.surface          = *ctx.surface;
    ci.minImageCount    = capabilities.minImageCount + 1;
    ci.imageFormat      = ctx.swapchain.format.format;
    ci.imageColorSpace  = ctx.swapchain.format.colorSpace;
    ci.imageExtent      = ctx.swapchain.extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc;

    if (ctx.graphics_family_idx != ctx.present_family_idx) {
        std::array indices{ctx.graphics_family_idx, ctx.present_family_idx};
        ci.imageSharingMode      = vk::SharingMode::eConcurrent;
        ci.queueFamilyIndexCount = indices.size();
        ci.pQueueFamilyIndices   = indices.data();
    }
    else {
        ci.imageSharingMode = vk::SharingMode::eExclusive;
    }

    ci.preTransform   = vk::SurfaceTransformFlagBitsKHR::eIdentity;
    ci.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    ci.presentMode    = ctx.swapchain.present_mode;
    ci.clipped        = VK_TRUE;

    ctx.swapchain.handle = ctx.device->createSwapchainKHRUnique(ci);
    ctx.swapchain.images = ctx.device->getSwapchainImagesKHR(*ctx.swapchain.handle);
    ctx.swapchain.image_views.resize(ctx.swapchain.images.size());

    for (size_t i = 0; i < ctx.swapchain.images.size(); i++) {
        vk::ImageViewCreateInfo ici{};
        ici.image    = ctx.swapchain.images[i];
        ici.viewType = vk::ImageViewType::e2D;
        ici.format   = ctx.swapchain.format.format;
        ici.components
            = {vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA};
        ici.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
        ici.subresourceRange.baseMipLevel   = 0;
        ici.subresourceRange.levelCount     = 1;
        ici.subresourceRange.baseArrayLayer = 0;
        ici.subresourceRange.layerCount     = 1;
        ctx.swapchain.image_views[i]        = ctx.device->createImageView(ici);
    }
}

void Context::destroy_swapchain()
{
    device->waitIdle();

    for (auto &o : swapchain.image_views) {
        device->destroy(o);
    }
}

void Context::create_frame_resources(usize count)
{
    frame_resources.current = 0;
    frame_resources.data.resize(count);

    for (usize i = 0; i < count; i++) {
        auto &frame_resource = frame_resources.data[i];

        frame_resource.fence              = device->createFenceUnique({vk::FenceCreateFlagBits::eSignaled});
        frame_resource.image_available    = device->createSemaphoreUnique({});
        frame_resource.rendering_finished = device->createSemaphoreUnique({});

        /// --- Create the command pool to create a command buffer for each frame
        frame_resource.command_pool = device->createCommandPoolUnique({{vk::CommandPoolCreateFlagBits::eTransient}, static_cast<uint32_t>(graphics_family_idx)});
    }
}

void Context::destroy()
{
    if (ENABLE_VALIDATION_LAYERS) {
        instance->destroyDebugUtilsMessengerEXT(*debug_messenger, nullptr);
    }

    destroy_swapchain();

    char *dump;
    vmaBuildStatsString(allocator, &dump, VK_TRUE);
    std::cout << "Vulkan memory dump:\n" << dump << "\n";
    vmaFreeStatsString(allocator, dump);

    vmaDestroyAllocator(allocator);
}

void Context::on_resize(int /*width*/, int /*height*/)
{
    destroy_swapchain();
    create_swapchain();

    for (auto& frame_resource : frame_resources.data)
    {
        frame_resource.fence = device->createFenceUnique({vk::FenceCreateFlagBits::eSignaled});
	device->resetCommandPool(*frame_resource.command_pool, {vk::CommandPoolResetFlagBits::eReleaseResources});
	frame_resource.command_buffer = std::move(device->allocateCommandBuffersUnique({*frame_resource.command_pool, vk::CommandBufferLevel::ePrimary, 1})[0]);
    }
}

} // namespace my_app::vulkan
