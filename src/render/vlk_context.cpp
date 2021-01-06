#if defined(_WIN32)
#    define VK_USE_PLATFORM_WIN32_KHR
#else
#    define VK_USE_PLATFORM_XCB_KHR
#endif

#define VMA_IMPLEMENTATION

#include "render/vlk_context.hpp"

#include "platform/window.hpp"

#include <string>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <fmt/core.h>

namespace my_app::vulkan
{

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                                                     VkDebugUtilsMessageTypeFlagsEXT /*message_type*/,
                                                     const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                     void * /*unused*/)
{
    fmt::print(stderr, "{}\n", pCallbackData->pMessage);

    if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) == 0)
    {
        return VK_FALSE;
    }

    if (pCallbackData->objectCount)
    {
        fmt::print(stderr, "Objects:\n");
        for (size_t i = 0; i < pCallbackData->objectCount; i++)
        {
            const auto &object = pCallbackData->pObjects[i];
            fmt::print(stderr, "\t [{}] {}\n", i, (object.pObjectName ? object.pObjectName : "NoName"));
        }
    }

    return VK_FALSE;
}

bool is_extension_installed(const char *wanted, const std::vector<VkExtensionProperties> &installed)
{
    for (const auto &extension : installed)
    {
        if (!strcmp(wanted, extension.extensionName))
        {
            return true;
        }
    }
    return false;
}

void Context::create(Context &ctx, const platform::Window &window)
{
    /// --- Create Instance
    std::vector<const char *> instance_extensions;

    instance_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    instance_extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    instance_extensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#else
    assert(false);
#endif

    instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    uint layer_props_count = 0;
    std::vector<VkLayerProperties> installed_instance_layers;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layer_props_count, nullptr));
    installed_instance_layers.resize(layer_props_count);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layer_props_count, installed_instance_layers.data()));

    std::vector<const char *> instance_layers;
    if (ENABLE_VALIDATION_LAYERS)
    {
        instance_layers.push_back("VK_LAYER_KHRONOS_validation");
    }

    VkApplicationInfo app_info  = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app_info.pApplicationName   = "Test Vulkan";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName        = "GoodEngine";
    app_info.engineVersion      = VK_MAKE_VERSION(1, 1, 0);
    app_info.apiVersion         = VK_API_VERSION_1_2;

    // TODO: add synchronization once the flag is available in vulkan_core.h
    std::array<VkValidationFeatureEnableEXT, 1> enables{VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT};

    VkValidationFeaturesEXT features       = {.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
    features.enabledValidationFeatureCount = enables.size();
    features.pEnabledValidationFeatures    = enables.data();

    VkInstanceCreateInfo create_info    = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create_info.pNext                   = &features;
    create_info.flags                   = 0;
    create_info.pApplicationInfo        = &app_info;
    create_info.enabledLayerCount       = static_cast<uint32_t>(instance_layers.size());
    create_info.ppEnabledLayerNames     = instance_layers.data();
    create_info.enabledExtensionCount   = static_cast<uint32_t>(instance_extensions.size());
    create_info.ppEnabledExtensionNames = instance_extensions.data();

    VK_CHECK(vkCreateInstance(&create_info, nullptr, &ctx.instance));

    /// --- Load instance functions
#define X(name) ctx.name = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(ctx.instance, #name))
    X(vkCreateDebugUtilsMessengerEXT);
    X(vkDestroyDebugUtilsMessengerEXT);
    X(vkCmdBeginDebugUtilsLabelEXT);
    X(vkCmdEndDebugUtilsLabelEXT);
    X(vkSetDebugUtilsObjectNameEXT);
#undef X

    /// --- Init debug layers
    if (ENABLE_VALIDATION_LAYERS)
    {
        VkDebugUtilsMessengerCreateInfoEXT ci = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        ci.flags                              = 0;
        ci.messageSeverity                    = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        ci.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        // ci.messageType     |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        ci.pfnUserCallback = debug_callback;

        VkDebugUtilsMessengerEXT messenger;
        VK_CHECK(ctx.vkCreateDebugUtilsMessengerEXT(ctx.instance, &ci, nullptr, &messenger));
        ctx.debug_messenger = messenger;
    }

    /// --- Create the surface
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    VkWin32SurfaceCreateInfoKHR sci = {.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    sci.hwnd                        = window.win32.window;
    sci.hinstance                   = GetModuleHandle(nullptr);
    VK_CHECK(vkCreateWin32SurfaceKHR(ctx.instance, &sci, nullptr, &ctx.surface));
#else
    VkXcbSurfaceCreateInfoKHR sci = {.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR};
    sci.connection                = window.xcb.connection;
    sci.window                    = window.xcb.window;
    VK_CHECK(vkCreateXcbSurfaceKHR(ctx.instance, &sci, nullptr, &ctx.surface));
#endif

    /// --- Pick a physical device
    uint physical_devices_count = 0;
    std::vector<VkPhysicalDevice> physical_devices;
    VK_CHECK(vkEnumeratePhysicalDevices(ctx.instance, &physical_devices_count, nullptr));
    physical_devices.resize(physical_devices_count);
    VK_CHECK(vkEnumeratePhysicalDevices(ctx.instance, &physical_devices_count, physical_devices.data()));

    ctx.physical_device = physical_devices[0];
    for (const auto &physical_device : physical_devices)
    {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physical_device, &properties);

        fmt::print("Device: {}\n", properties.deviceName);
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            fmt::print("Selected: {}\n", properties.deviceName);
            ctx.physical_device = physical_device;
        }
    }

    /// --- Create the logical device
    std::vector<VkExtensionProperties> installed_device_extensions;
    uint installed_device_extensions_count = 0;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(ctx.physical_device,
                                                  nullptr,
                                                  &installed_device_extensions_count,
                                                  nullptr));
    installed_device_extensions.resize(installed_device_extensions_count);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(ctx.physical_device,
                                                  nullptr,
                                                  &installed_device_extensions_count,
                                                  installed_device_extensions.data()));

    std::vector<const char *> device_extensions;
    device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    device_extensions.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
    if (is_extension_installed(VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME, installed_device_extensions))
    {
        device_extensions.push_back(VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME);
    }

    ctx.vulkan12_features              = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    ctx.physical_device_features       = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    ctx.physical_device_features.pNext = &ctx.vulkan12_features;
    vkGetPhysicalDeviceFeatures2(ctx.physical_device, &ctx.physical_device_features);

    uint queue_families_count = 0;
    std::vector<VkQueueFamilyProperties> queue_families;
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physical_device, &queue_families_count, nullptr);
    queue_families.resize(queue_families_count);
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physical_device, &queue_families_count, queue_families.data());

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    float priority = 0.0;

    bool has_graphics = false;
    bool has_present  = false;
    for (uint32_t i = 0; i < queue_families.size(); i++)
    {
        if (!has_graphics && queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            // Create a single graphics queue.
            queue_create_infos.emplace_back();
            auto &queue_create_info            = queue_create_infos.back();
            queue_create_info                  = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queue_create_info.queueFamilyIndex = i;
            queue_create_info.queueCount       = 1;
            queue_create_info.pQueuePriorities = &priority;
            has_graphics                       = true;
            ctx.graphics_family_idx            = i;
        }

        VkBool32 surface_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(ctx.physical_device, i, ctx.surface, &surface_support);

        if (!has_present && surface_support)
        {
            // Create a single graphics queue.
            queue_create_infos.emplace_back();
            auto &queue_create_info            = queue_create_infos.back();
            queue_create_info                  = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queue_create_info.queueFamilyIndex = i;
            queue_create_info.queueCount       = 1;
            queue_create_info.pQueuePriorities = &priority;
            has_present                        = true;
            ctx.present_family_idx             = i;
        }
    }

    if (!has_present || !has_graphics)
    {
        throw std::runtime_error("failed to find a graphics and present queue.");
    }

    if (ctx.present_family_idx == ctx.graphics_family_idx)
    {
        queue_create_infos.pop_back();
    }

    VkDeviceCreateInfo dci      = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.pNext                   = &ctx.physical_device_features;
    dci.flags                   = 0;
    dci.queueCreateInfoCount    = static_cast<uint32_t>(queue_create_infos.size());
    dci.pQueueCreateInfos       = queue_create_infos.data();
    dci.enabledLayerCount       = 0;
    dci.ppEnabledLayerNames     = nullptr;
    dci.enabledExtensionCount   = static_cast<uint32_t>(device_extensions.size());
    dci.ppEnabledExtensionNames = device_extensions.data();
    dci.pEnabledFeatures        = nullptr;

    VK_CHECK(vkCreateDevice(ctx.physical_device, &dci, nullptr, &ctx.device));

    vkGetPhysicalDeviceProperties(ctx.physical_device, &ctx.physical_props);

    /// --- Init VMA allocator
    VmaAllocatorCreateInfo allocator_info = {};
    allocator_info.vulkanApiVersion       = VK_API_VERSION_1_2;
    allocator_info.physicalDevice         = ctx.physical_device;
    allocator_info.device                 = ctx.device;
    allocator_info.instance               = ctx.instance;
    allocator_info.flags                  = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    VK_CHECK(vmaCreateAllocator(&allocator_info, &ctx.allocator));

    /// --- Create the swapchain
    ctx.create_swapchain();

    ctx.create_frame_resources(FRAMES_IN_FLIGHT);

    /// --- The descriptor sets of the pool are recycled manually
    std::array pool_sizes{
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 16 * 1024},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          .descriptorCount = 16 * 1024},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount = 16 * 1024},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         .descriptorCount = 16 * 1024},
    };

    VkDescriptorPoolCreateInfo dpci = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpci.flags                      = 0;
    dpci.poolSizeCount              = pool_sizes.size();
    dpci.pPoolSizes                 = pool_sizes.data();
    dpci.maxSets                    = 2 * 1024;
    VK_CHECK(vkCreateDescriptorPool(ctx.device, &dpci, nullptr, &ctx.descriptor_pool));

    VkQueryPoolCreateInfo qpci = {.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    qpci.queryType             = VK_QUERY_TYPE_TIMESTAMP;
    qpci.queryCount            = FRAMES_IN_FLIGHT * MAX_TIMESTAMP_PER_FRAME;
    VK_CHECK(vkCreateQueryPool(ctx.device, &qpci, nullptr, &ctx.timestamp_pool));
}

void Context::create_swapchain()
{
    auto &ctx = *this;

    // Use default extent for the swapchain
    VkSurfaceCapabilitiesKHR capabilities;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.physical_device, ctx.surface, &capabilities));
    ctx.swapchain.extent = capabilities.currentExtent;

    // Find a good present mode (by priority Mailbox then Immediate then FIFO)
    uint present_modes_count = 0;
    std::vector<VkPresentModeKHR> present_modes;
    VK_CHECK(
        vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.physical_device, ctx.surface, &present_modes_count, nullptr));
    present_modes.resize(present_modes_count);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.physical_device,
                                                       ctx.surface,
                                                       &present_modes_count,
                                                       present_modes.data()));
    ctx.swapchain.present_mode = VK_PRESENT_MODE_FIFO_KHR;

    for (auto &pm : present_modes)
    {
        if (pm == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            ctx.swapchain.present_mode = pm;
            break;
        }
    }

    if (ctx.swapchain.present_mode == VK_PRESENT_MODE_FIFO_KHR)
    {
        for (auto &pm : present_modes)
        {
            if (pm == VK_PRESENT_MODE_IMMEDIATE_KHR)
            {
                ctx.swapchain.present_mode = pm;
                break;
            }
        }
    }

    // Find the best format
    uint formats_count = 0;
    std::vector<VkSurfaceFormatKHR> formats;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physical_device, ctx.surface, &formats_count, nullptr));
    formats.resize(formats_count);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physical_device, ctx.surface, &formats_count, formats.data()));
    ctx.swapchain.format = formats[0];
    if (ctx.swapchain.format.format == VK_FORMAT_UNDEFINED)
    {
        ctx.swapchain.format.format     = VK_FORMAT_B8G8R8A8_UNORM;
        ctx.swapchain.format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    }
    else
    {
        for (const auto &f : formats)
        {
            if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                ctx.swapchain.format = f;
                break;
            }
        }
    }

    auto image_count = capabilities.minImageCount + 2u;
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount)
    {
        image_count = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR ci = {.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    ci.surface                  = ctx.surface;
    ci.minImageCount            = image_count;
    ci.imageFormat              = ctx.swapchain.format.format;
    ci.imageColorSpace          = ctx.swapchain.format.colorSpace;
    ci.imageExtent              = ctx.swapchain.extent;
    ci.imageArrayLayers         = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                    | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if (ctx.graphics_family_idx != ctx.present_family_idx)
    {
        std::array indices{ctx.graphics_family_idx, ctx.present_family_idx};
        ci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = indices.size();
        ci.pQueueFamilyIndices   = indices.data();
    }
    else
    {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    ci.preTransform   = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode    = ctx.swapchain.present_mode;
    ci.clipped        = VK_TRUE;

    VK_CHECK(vkCreateSwapchainKHR(ctx.device, &ci, nullptr, &ctx.swapchain.handle));

    ctx.swapchain.images_count = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(ctx.device, ctx.swapchain.handle, &ctx.swapchain.images_count, nullptr));
    ctx.swapchain.images.resize(ctx.swapchain.images_count);
    VK_CHECK(vkGetSwapchainImagesKHR(ctx.device,
                                     ctx.swapchain.handle,
                                     &ctx.swapchain.images_count,
                                     ctx.swapchain.images.data()));
}

void Context::destroy_swapchain()
{
    vkDestroySwapchainKHR(device, swapchain.handle, nullptr);
    swapchain.handle = VK_NULL_HANDLE;
}

void Context::create_frame_resources(usize count)
{
    frame_resources.current = 0;
    frame_resources.data.resize(count);

    for (usize i = 0; i < count; i++)
    {
        auto &frame_resource = frame_resources.data[i];

        VkFenceCreateInfo fci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fci.flags             = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_CHECK(vkCreateFence(device, &fci, nullptr, &frame_resource.fence));

        VkSemaphoreCreateInfo sci = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VK_CHECK(vkCreateSemaphore(device, &sci, nullptr, &frame_resource.image_available));
        VK_CHECK(vkCreateSemaphore(device, &sci, nullptr, &frame_resource.rendering_finished));

        /// --- Create the command pool to create a command buffer for each frame
        VkCommandPoolCreateInfo cpci = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        cpci.flags                   = 0;
        cpci.queueFamilyIndex        = graphics_family_idx;
        VK_CHECK(vkCreateCommandPool(device, &cpci, nullptr, &frame_resource.command_pool));
    }
}

void Context::destroy()
{
    VK_CHECK(vkDeviceWaitIdle(device));

    vkDestroyQueryPool(device, timestamp_pool, nullptr);
    vkDestroyDescriptorPool(device, descriptor_pool, nullptr);

    for (auto &frame : frame_resources.data)
    {
        vkDestroyFence(device, frame.fence, nullptr);
        vkDestroySemaphore(device, frame.image_available, nullptr);
        vkDestroySemaphore(device, frame.rendering_finished, nullptr);
        vkDestroyCommandPool(device, frame.command_pool, nullptr);
    }

    destroy_swapchain();

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vmaDestroyAllocator(allocator);
    vkDestroyDevice(device, nullptr);

    if (ENABLE_VALIDATION_LAYERS)
    {
        vkDestroyDebugUtilsMessengerEXT(instance, *debug_messenger, nullptr);
    }

    vkDestroyInstance(instance, nullptr);
}

void Context::on_resize(int /*window_width*/, int /*window_height*/)
{
    VK_CHECK(vkDeviceWaitIdle(device));

    destroy_swapchain();
    create_swapchain();

    for (auto &frame_resource : frame_resources.data)
    {
        // TODO: isnt it possible to reset then signal the fence?
        vkDestroyFence(device, frame_resource.fence, nullptr);

        VkFenceCreateInfo fci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fci.flags             = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_CHECK(vkCreateFence(device, &fci, nullptr, &frame_resource.fence));

        VK_CHECK(vkResetCommandPool(device, frame_resource.command_pool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT));

        VkCommandBufferAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool                 = frame_resource.command_pool;
        ai.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount          = 1;
        VK_CHECK(vkAllocateCommandBuffers(device, &ai, &frame_resource.command_buffer));
    }
}

} // namespace my_app::vulkan

bool operator==(const VkPipelineShaderStageCreateInfo &a, const VkPipelineShaderStageCreateInfo &b)
{
    return a.flags == b.flags && a.stage == b.stage && a.module == b.module && a.pName == b.pName // TODO: strcmp?
           && a.pSpecializationInfo == b.pSpecializationInfo;                                     // TODO: deep cmp?
}

bool operator==(const VkDescriptorBufferInfo &a, const VkDescriptorBufferInfo &b)
{
    return a.buffer == b.buffer && a.offset == b.offset && a.range == b.range;
}

bool operator==(const VkDescriptorImageInfo &a, const VkDescriptorImageInfo &b)
{
    return a.sampler == b.sampler && a.imageView == b.imageView && a.imageLayout == b.imageLayout;
}

bool operator==(const VkExtent3D &a, const VkExtent3D &b)
{
    return a.width == b.width && a.height == b.height && a.depth == b.depth;
}

bool operator==(const VkImageSubresourceRange &a, const VkImageSubresourceRange &b)
{
    return a.aspectMask == b.aspectMask && a.baseMipLevel == b.baseMipLevel && a.levelCount == b.levelCount
           && a.baseArrayLayer == b.baseArrayLayer && a.layerCount == b.layerCount;
}

bool operator==(const VkImageCreateInfo &a, const VkImageCreateInfo &b)
{
    bool same = a.queueFamilyIndexCount == b.queueFamilyIndexCount;
    if (!same)
    {
        return false;
    }

    if (a.pQueueFamilyIndices && b.pQueueFamilyIndices)
    {
        for (usize i = 0; i < a.queueFamilyIndexCount; i++)
        {
            if (a.pQueueFamilyIndices[i] != b.pQueueFamilyIndices[i])
            {
                return false;
            }
        }
    }
    else
    {
        same = a.pQueueFamilyIndices == b.pQueueFamilyIndices;
    }

    return same && a.flags == b.flags && a.imageType == b.imageType && a.format == b.format && a.extent == b.extent
           && a.mipLevels == b.mipLevels && a.arrayLayers == b.arrayLayers && a.samples == b.samples
           && a.tiling == b.tiling && a.usage == b.usage && a.sharingMode == b.sharingMode
           && a.initialLayout == b.initialLayout;
}

bool operator==(const VkComputePipelineCreateInfo &a, const VkComputePipelineCreateInfo &b)
{
    return a.flags == b.flags && a.stage == b.stage && a.layout == b.layout
           && a.basePipelineHandle == b.basePipelineHandle && a.basePipelineIndex == b.basePipelineIndex;
}

bool operator==(const VkFramebufferCreateInfo &a, const VkFramebufferCreateInfo &b)
{
    if (a.attachmentCount != b.attachmentCount)
    {
        return false;
    }

    for (uint i = 0; i < a.attachmentCount; i++)
    {
        if (a.pAttachments[i] != b.pAttachments[i])
        {
            return false;
        }
    }

    return a.flags == b.flags && a.renderPass == b.renderPass && a.width == b.width && a.height == b.height
           && a.layers == b.layers;
}
