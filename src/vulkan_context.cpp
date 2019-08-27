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

        char* message;
        vmaBuildStatsString(allocator, &message, VK_TRUE);
        std::cout << message << std::endl;
        vmaFreeStatsString(allocator, message);

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

    void VulkanContext::CopyDataToImage(void const* data, uint32_t data_size, Image& target_image, uint32_t width, uint32_t height, vk::ImageSubresourceRange const& image_subresource_range, vk::ImageLayout current_image_layout, vk::AccessFlags current_image_access, vk::PipelineStageFlags generating_stages, vk::ImageLayout new_image_layout, vk::AccessFlags new_image_access, vk::PipelineStageFlags consuming_stages) const
    {
        // Create staging buffer and map it's memory to copy data from the CPU

        std::string name = "Staging buffer CopyDataToImage for size ";
        name += std::to_string(data_size);
        Buffer staging_buffer{ name, allocator, data_size, vk::BufferUsageFlagBits::eTransferSrc };
        void* mapped = staging_buffer.map();
        std::memcpy(mapped, data, data_size);

        // Allocate temporary command buffer from a temporary command pool
        vk::UniqueCommandPool command_pool = device->createCommandPoolUnique({ { vk::CommandPoolCreateFlagBits::eTransient }, static_cast<uint32_t>(graphics_family_idx) });
        vk::CommandBufferAllocateInfo cbai{};
        vk::UniqueCommandBuffer cmd = std::move(device->allocateCommandBuffersUnique({ command_pool.get(), vk::CommandBufferLevel::ePrimary, 1 })[0]);

        // Record command buffer which copies data from the staging buffer to the destination buffer
        cmd->begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

        vk::ImageMemoryBarrier pre_transfer_image_memory_barrier(
            current_image_access,                    // VkAccessFlags                          srcAccessMask
            vk::AccessFlagBits::eTransferWrite,      // VkAccessFlags                          dstAccessMask
            current_image_layout,                    // VkImageLayout                          oldLayout
            vk::ImageLayout::eTransferDstOptimal,    // VkImageLayout                          newLayout
            VK_QUEUE_FAMILY_IGNORED,                 // uint32_t                               srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                 // uint32_t                               dstQueueFamilyIndex
            target_image.get_image(),                // VkImage                                image
            image_subresource_range                  // VkImageSubresourceRange                subresourceRange
        );
        cmd->pipelineBarrier(generating_stages, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(0), {}, {}, { pre_transfer_image_memory_barrier });

        std::vector<vk::BufferImageCopy> buffer_image_copy;
        buffer_image_copy.reserve(image_subresource_range.levelCount);
        for (uint32_t i = image_subresource_range.baseMipLevel; i < image_subresource_range.baseMipLevel + image_subresource_range.levelCount; ++i)
        {
            buffer_image_copy.emplace_back(
                0,                                             // VkDeviceSize                           bufferOffset
                0,                                             // uint32_t                               bufferRowLength
                0,                                             // uint32_t                               bufferImageHeight
                vk::ImageSubresourceLayers(                    // VkImageSubresourceLayers               imageSubresource
                    image_subresource_range.aspectMask,        // VkImageAspectFlags                     aspectMask
                    i,                                         // uint32_t                               mipLevel
                    image_subresource_range.baseArrayLayer,    // uint32_t                               baseArrayLayer
                    image_subresource_range.layerCount         // uint32_t                               layerCount
                    ),
                vk::Offset3D(),    // VkOffset3D                             imageOffset
                vk::Extent3D(      // VkExtent3D                             imageExtent
                    width,         // uint32_t                               width
                    height,        // uint32_t                               height
                    1              // uint32_t                               depth
                    ));
        }
        cmd->copyBufferToImage(staging_buffer.get_buffer(), target_image.get_image(), vk::ImageLayout::eTransferDstOptimal, buffer_image_copy);

        vk::ImageMemoryBarrier post_transfer_image_memory_barrier(
            vk::AccessFlagBits::eTransferWrite,      // VkAccessFlags                          srcAccessMask
            new_image_access,                        // VkAccessFlags                          dstAccessMask
            vk::ImageLayout::eTransferDstOptimal,    // VkImageLayout                          oldLayout
            new_image_layout,                        // VkImageLayout                          newLayout
            VK_QUEUE_FAMILY_IGNORED,                 // uint32_t                               srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                 // uint32_t                               dstQueueFamilyIndex
            target_image.get_image(),                // VkImage                                image
            image_subresource_range                  // VkImageSubresourceRange                subresourceRange
        );
        cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, consuming_stages, vk::DependencyFlags(0), {}, {}, { post_transfer_image_memory_barrier });

        cmd->end();

        // Submit
        vk::UniqueFence fence = device->createFenceUnique({});

        vk::SubmitInfo si{};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd.get();
        get_graphics_queue().submit({ si }, fence.get());
        device->waitForFences({ fence.get() }, VK_FALSE, UINT64_MAX);

        staging_buffer.free();
    }

    void VulkanContext::CopyDataToBuffer(void const* data, uint32_t data_size, Buffer buffer, vk::AccessFlags current_buffer_access, vk::PipelineStageFlags generating_stages, vk::AccessFlags new_buffer_access, vk::PipelineStageFlags consuming_stages) const
    {
        // Create staging buffer and map it's memory to copy data from the CPU
        std::string name = "Staging buffer CopyDataToBuffer for size ";
        name += std::to_string(data_size);
        Buffer staging_buffer{ name, allocator, data_size, vk::BufferUsageFlagBits::eTransferSrc };
        void* mapped = staging_buffer.map();
        std::memcpy(mapped, data, data_size);

        // Allocate temporary command buffer from a temporary command pool
        vk::UniqueCommandPool command_pool = device->createCommandPoolUnique({ { vk::CommandPoolCreateFlagBits::eTransient }, static_cast<uint32_t>(graphics_family_idx) });
        vk::UniqueCommandBuffer cmd = std::move(device->allocateCommandBuffersUnique({ command_pool.get(), vk::CommandBufferLevel::ePrimary, 1 })[0]);

        // Record command buffer which copies data from the staging buffer to the destination buffer
        cmd->begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

        vk::BufferMemoryBarrier bmb_pre{};
        bmb_pre.srcAccessMask = current_buffer_access;
        bmb_pre.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        bmb_pre.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bmb_pre.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bmb_pre.buffer = buffer.get_buffer();
        bmb_pre.offset = 0;
        bmb_pre.size = data_size;
        cmd->pipelineBarrier(generating_stages, vk::PipelineStageFlagBits::eTransfer, {}, {}, { bmb_pre }, {});

        vk::BufferCopy bcp{};
        bcp.srcOffset = 0;
        bcp.dstOffset = 0;
        bcp.size = data_size;
        cmd->copyBuffer(staging_buffer.get_buffer(), buffer.get_buffer(), { bcp });

        vk::BufferMemoryBarrier bmb_post{};
        bmb_post.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        bmb_post.dstAccessMask = new_buffer_access;
        bmb_post.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bmb_post.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bmb_post.buffer = buffer.get_buffer();
        bmb_post.offset = 0;
        bmb_post.size = data_size;
        cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, consuming_stages, {}, {}, { bmb_post }, {});
        cmd->end();

        // Submit
        vk::UniqueFence fence = device->createFenceUnique({});

        vk::SubmitInfo si{};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd.get();
        get_graphics_queue().submit({ si }, fence.get());
        device->waitForFences({ fence.get() }, VK_FALSE, UINT64_MAX);

        staging_buffer.free();
    }
}    // namespace my_app
