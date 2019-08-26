#pragma clang diagnostic ignored "-Weverything"
#include <GLFW/glfw3.h>
#include <iostream>
#pragma clang diagnostic pop

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

    std::vector<const char*> get_extensions()
    {
        auto installed = vk::enumerateInstanceExtensionProperties();

        std::vector<const char*> extensions;

        uint32_t required_count;
        const char** required_extensions = glfwGetRequiredInstanceExtensions(&required_count);
        for (auto i = 0; i < required_count; i++)
            extensions.push_back(required_extensions[i]);

        if (ENABLE_VALIDATION_LAYERS)
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        return extensions;
    }

    std::vector<const char*> get_layers()
    {
        std::vector<const char*> layers;

        auto installed = vk::enumerateInstanceLayerProperties();

        for (auto& layer : installed)
            for (auto& wanted : VALIDATION_LAYERS)
                if (std::string(layer.layerName).compare(wanted) == 0)
                    layers.push_back(wanted);

        return layers;
    }

    VulkanContext::VulkanContext(GLFWwindow* window)
        : graphics_family_idx(0)
        , present_family_idx(0)
        , instance(create_instance())
        , dldi(vk::DispatchLoaderDynamic(*instance))
        , debug_messenger(setup_messenger())
        , surface(create_surface(window))
        , physical_device(pick_physical_device())
        , device(create_logical_device())
        , allocator(init_allocator())
        , command_pool(device->createCommandPool({ { vk::CommandPoolCreateFlagBits::eResetCommandBuffer }, static_cast<uint32_t>(graphics_family_idx) }))
    {
    }

    VulkanContext::~VulkanContext()
    {
        if (debug_messenger)
            instance->destroyDebugUtilsMessengerEXT(*debug_messenger, nullptr, dldi);

        device->destroy(command_pool);
        vmaDestroyAllocator(allocator);
    }

    vk::UniqueInstance VulkanContext::create_instance()
    {
        vk::ApplicationInfo app_info;
        app_info.pApplicationName = "Test Vulkan";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName = "GoodEngine";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_1;

        auto extensions = get_extensions();
        auto layers = get_layers();

        vk::InstanceCreateInfo create_info;
        create_info.flags = {};
        create_info.pApplicationInfo = &app_info;
        create_info.enabledLayerCount = layers.size();
        create_info.ppEnabledLayerNames = layers.data();
        create_info.enabledExtensionCount = extensions.size();
        create_info.ppEnabledExtensionNames = extensions.data();

        return vk::createInstanceUnique(create_info);
    }

    std::optional<vk::DebugUtilsMessengerEXT> VulkanContext::setup_messenger()
    {
        if (!ENABLE_VALIDATION_LAYERS)
            return std::nullopt;

        vk::DebugUtilsMessengerCreateInfoEXT ci;
        ci.flags = {};
        ci.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        ci.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
        ci.pfnUserCallback = DebugCallback;

        return instance->createDebugUtilsMessengerEXT(ci, nullptr, dldi);
    }

    vk::UniqueSurfaceKHR VulkanContext::create_surface(GLFWwindow* window)
    {
        VkSurfaceKHR surfaceTmp;
        glfwCreateWindowSurface(static_cast<VkInstance>(*instance), window, nullptr, &surfaceTmp);
        return vk::UniqueSurfaceKHR(surfaceTmp, *instance);
    }

    vk::PhysicalDevice VulkanContext::pick_physical_device()
    {
        auto physical_devices = instance->enumeratePhysicalDevices();
        vk::PhysicalDevice selected = physical_devices[0];
        for (const auto& device : physical_devices)
        {
            auto properties = device.getProperties();
            std::cout << "Device: " << properties.deviceName << "\n";
            if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
            {
                std::cout << "Selected: " << properties.deviceName << "\n";
                selected = device;
            }
        }
        return selected;
    }

    vk::UniqueDevice VulkanContext::create_logical_device()
    {
        std::vector<const char*> extensions;
        auto installed_ext = physical_device.enumerateDeviceExtensionProperties();
        for (const auto& wanted : DEVICE_EXTENSIONS)
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
        for (const auto& wanted : VALIDATION_LAYERS)
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

    VmaAllocator VulkanContext::init_allocator()
    {
        VmaAllocator result;
        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.physicalDevice = physical_device;
        allocatorInfo.device = *device;
        vmaCreateAllocator(&allocatorInfo, &result);
        return result;
    }

    void VulkanContext::transition_layout(vk::PipelineStageFlagBits src, vk::PipelineStageFlagBits dst, vk::ImageMemoryBarrier barrier) const
    {
        vk::CommandBuffer cmd = device->allocateCommandBuffers({ command_pool, vk::CommandBufferLevel::ePrimary, 1 })[0];

        cmd.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
        cmd.pipelineBarrier(
            src,
            dst,
            {},
            nullptr,
            nullptr,
            barrier);

        cmd.end();

        vk::Fence fence = device->createFence({});
        vk::SubmitInfo submit{};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;

        get_graphics_queue().submit(submit, fence);
        device->waitForFences(fence, VK_TRUE, UINT64_MAX);
        device->destroy(fence);
    }


    vk::UniqueShaderModule VulkanContext::create_shader_module(std::vector<char> code) const
    {
        vk::ShaderModuleCreateInfo info{};
        info.codeSize = code.size();
        info.pCode = reinterpret_cast<const uint32_t*>(code.data());
        return device->createShaderModuleUnique(info);
    }

    vk::Queue VulkanContext::get_graphics_queue() const
    {
        return device->getQueue(graphics_family_idx, 0);
    }

    vk::Queue VulkanContext::get_present_queue() const
    {
        return device->getQueue(present_family_idx, 0);
    }


    vk::UniqueDescriptorSetLayout VulkanContext::create_descriptor_layout(std::vector<vk::DescriptorSetLayoutBinding> bindings) const
    {
            vk::DescriptorSetLayoutCreateInfo dslci{};
            dslci.bindingCount = bindings.size();
            dslci.pBindings = bindings.data();
            return device->createDescriptorSetLayoutUnique(dslci);
    }
}    // namespace my_app
