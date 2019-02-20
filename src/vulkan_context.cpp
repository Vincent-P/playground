#include <iostream>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vulkan_context.hpp"

namespace my_app
{
    static VKAPI_ATTR VkBool32 VKAPI_CALL
    DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                  VkDebugUtilsMessageTypeFlagsEXT messageType,
                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
    {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }

    VulkanContext::VulkanContext(GLFWwindow* window)
    {
        CreateInstance();
        SetupDebugCallback();
        CreateSurface(window);
        CreateLogicalDevice();
        InitAllocator();
    }

    VulkanContext::~VulkanContext()
    {
        if (enable_validation_layers)
            instance->destroyDebugUtilsMessengerEXT(debug_messenger, nullptr, dldi);
    }

    std::vector<const char*> GetExtensions()
    {
        auto installed = vk::enumerateInstanceExtensionProperties();

        std::vector<const char*> extensions;

        uint32_t required_count;
        const char** required_extensions = glfwGetRequiredInstanceExtensions(&required_count);
        for (auto i = 0; i < required_count; i++)
            extensions.push_back(required_extensions[i]);

        if (enable_validation_layers)
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        return extensions;
    }

    std::vector<const char*> GetLayers()
    {
        std::vector<const char*> layers;

        auto installed = vk::enumerateInstanceLayerProperties();

        for (auto& layer : installed)
            for (auto& wanted : g_validation_layers)
                if (std::string(layer.layerName).compare(wanted) == 0)
                    layers.push_back(wanted);

        return layers;
    }

    void VulkanContext::CreateInstance()
    {
        vk::ApplicationInfo app_info;
        app_info.pApplicationName = "MyApp";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName = "MyEngine";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_1;

        auto extensions = GetExtensions();
        auto layers = GetLayers();

        vk::InstanceCreateInfo create_info;
        create_info.flags = {};
        create_info.pApplicationInfo = &app_info;
        create_info.enabledLayerCount = layers.size();
        create_info.ppEnabledLayerNames = layers.data();
        create_info.enabledExtensionCount = extensions.size();
        create_info.ppEnabledExtensionNames = extensions.data();

        instance = vk::createInstanceUnique(create_info);
        dldi = vk::DispatchLoaderDynamic(*instance);
    }

    void VulkanContext::SetupDebugCallback()
    {
        if (!enable_validation_layers)
            return;

        vk::DebugUtilsMessengerCreateInfoEXT ci;
        ci.flags = {};
        ci.messageSeverity =
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        ci.messageType =
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
        ci.pfnUserCallback = DebugCallback;

        debug_messenger = instance->createDebugUtilsMessengerEXT(ci, nullptr, dldi);
    }

    void VulkanContext::CreateSurface(GLFWwindow* window)
    {
        VkSurfaceKHR surfaceTmp;
        glfwCreateWindowSurface(static_cast<VkInstance>(*instance), window, nullptr, &surfaceTmp);
        surface = vk::UniqueSurfaceKHR(surfaceTmp, *instance);
    }

    void VulkanContext::CreateLogicalDevice()
    {
        auto physical_devices = instance->enumeratePhysicalDevices();
        auto gpu = *std::find_if(physical_devices.begin(), physical_devices.end(),
                                 [](const vk::PhysicalDevice& d) -> auto {
                                     return d.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
                                 });

        std::vector<const char*> extensions;
        auto installed_ext = gpu.enumerateDeviceExtensionProperties();
        for (const auto& wanted : g_device_extensions)
        {
            for (const auto& extension : installed_ext)
            {
                if (std::string(extension.extensionName).compare(wanted) == 0)
                {
                    extensions.push_back(wanted);
                    break;
                }
            }
        }

        std::vector<const char*> layers;
        auto installed_lay = gpu.enumerateDeviceLayerProperties();
        for (const auto& wanted : g_validation_layers)
        {
            for (auto& layer : installed_lay)
            {
                if (std::string(layer.layerName).compare(wanted) == 0)
                {
                    layers.push_back(wanted);
                    break;
                }
            }
        }

        auto features = gpu.getFeatures();
        auto queue_families = gpu.getQueueFamilyProperties();

        std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
        float priority = 0.0;

        bool has_graphics = false;
        bool has_present = false;
        for (size_t i = 0; i < queue_families.size(); i++)
        {
            if (!has_graphics && queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics)
            {
                // Create a single graphics queue.
                queue_create_infos.push_back(
                    vk::DeviceQueueCreateInfo(vk::DeviceQueueCreateFlags(), i, 1, &priority));
                has_graphics = true;
                graphics_family_idx = i;
            }

            if (!has_present && gpu.getSurfaceSupportKHR(i, *surface))
            {
                // Create a single graphics queue.
                queue_create_infos.push_back(
                    vk::DeviceQueueCreateInfo(vk::DeviceQueueCreateFlags(), i, 1, &priority));
                has_present = true;
                present_family_idx = i;
            }
        }

        if (!has_present || !has_graphics)
            throw std::runtime_error("failed to find a graphics and present queue.");

        if (present_family_idx == graphics_family_idx)
        {
            queue_create_infos.pop_back();
        }

        physical_device = gpu;

        vk::DeviceCreateInfo dci;
        dci.flags = {};
        dci.queueCreateInfoCount = queue_create_infos.size();
        dci.pQueueCreateInfos = queue_create_infos.data();
        dci.enabledLayerCount = layers.size();
        dci.ppEnabledLayerNames = layers.data();
        dci.enabledExtensionCount = extensions.size();
        dci.ppEnabledExtensionNames = extensions.data();
        dci.pEnabledFeatures = &features;

        device = gpu.createDeviceUnique(dci);
    }

    void VulkanContext::InitAllocator()
    {
        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.physicalDevice = physical_device;
        allocatorInfo.device = *device;
        vmaCreateAllocator(&allocatorInfo, &allocator);
    }

}    // namespace my_app
