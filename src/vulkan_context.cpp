#include <iostream>
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

    VulkanContext::VulkanContext(GLFWwindow* window)
        : instance(CreateInstance())
        , dldi(vk::DispatchLoaderDynamic(*instance))
        , debug_messenger(SetupDebugCallback())
        , surface(CreateSurface(window))
        , physical_device(PickPhysicalDevice())
        , device(CreateLogicalDevice())
        , allocator(InitAllocator())
        , command_pool(device->createCommandPool({{vk::CommandPoolCreateFlagBits::eResetCommandBuffer}, static_cast<uint32_t>(graphics_family_idx)}))
    {}

    VulkanContext::~VulkanContext()
    {
        if (debug_messenger)
            instance->destroyDebugUtilsMessengerEXT(*debug_messenger, nullptr, dldi);
        device->destroy(command_pool);

        vmaDestroyAllocator(allocator);
    }

    vk::UniqueInstance VulkanContext::CreateInstance()
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

        return vk::createInstanceUnique(create_info);
    }

    std::optional<vk::DebugUtilsMessengerEXT> VulkanContext::SetupDebugCallback()
    {
        if (!enable_validation_layers)
            return std::nullopt;

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

        return instance->createDebugUtilsMessengerEXT(ci, nullptr, dldi);
    }

    vk::UniqueSurfaceKHR VulkanContext::CreateSurface(GLFWwindow* window)
    {
        VkSurfaceKHR surfaceTmp;
        glfwCreateWindowSurface(static_cast<VkInstance>(*instance), window, nullptr, &surfaceTmp);
        return vk::UniqueSurfaceKHR(surfaceTmp, *instance);
    }

    vk::PhysicalDevice VulkanContext::PickPhysicalDevice()
    {
        auto physical_devices = instance->enumeratePhysicalDevices();
        return *std::find_if(physical_devices.begin(), physical_devices.end(),
                                 [](const vk::PhysicalDevice& d) -> auto {
                                     return d.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
                                 });
    }

    vk::UniqueDevice VulkanContext::CreateLogicalDevice()
    {
        std::vector<const char*> extensions;
        auto installed_ext = physical_device.enumerateDeviceExtensionProperties();
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
        auto installed_lay = physical_device.enumerateDeviceLayerProperties();
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

        auto features = physical_device.getFeatures();
        auto queue_families = physical_device.getQueueFamilyProperties();

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

            if (!has_present && physical_device.getSurfaceSupportKHR(i, *surface))
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
            queue_create_infos.pop_back();

        vk::DeviceCreateInfo dci;
        dci.flags = {};
        dci.queueCreateInfoCount = queue_create_infos.size();
        dci.pQueueCreateInfos = queue_create_infos.data();
        dci.enabledLayerCount = layers.size();
        dci.ppEnabledLayerNames = layers.data();
        dci.enabledExtensionCount = extensions.size();
        dci.ppEnabledExtensionNames = extensions.data();
        dci.pEnabledFeatures = &features;

        return physical_device.createDeviceUnique(dci);
    }

    VmaAllocator VulkanContext::InitAllocator()
    {
        VmaAllocator result;
        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.physicalDevice = physical_device;
        allocatorInfo.device = *device;
        vmaCreateAllocator(&allocatorInfo, &result);
        return result;
    }

}    // namespace my_app
