#include "vulkan_context.hpp"

#include <GLFW/glfw3.h>
#include <iostream>
#include <thsvs/thsvs_simpler_vulkan_synchronization.h>

namespace my_app
{
    static constexpr const char* to_string(const vk::ObjectType type)
    {
        switch (type)
        {
        case vk::ObjectType::eUnknown: return "Unknown";
        case vk::ObjectType::eInstance: return "Instance";
        case vk::ObjectType::ePhysicalDevice: return "PhysicalDevice";
        case vk::ObjectType::eDevice: return "Device";
        case vk::ObjectType::eQueue: return "Queue";
        case vk::ObjectType::eSemaphore: return "Semaphore";
        case vk::ObjectType::eCommandBuffer: return "CommandBuffer";
        case vk::ObjectType::eFence: return "Fence";
        case vk::ObjectType::eDeviceMemory: return "DeviceMemory";
        case vk::ObjectType::eBuffer: return "Buffer";
        case vk::ObjectType::eImage: return "Image";
        case vk::ObjectType::eEvent: return "Event";
        case vk::ObjectType::eQueryPool: return "QueryPool";
        case vk::ObjectType::eBufferView: return "BufferView";
        case vk::ObjectType::eImageView: return "ImageView";
        case vk::ObjectType::eShaderModule: return "ShaderModule";
        case vk::ObjectType::ePipelineCache: return "PipelineCache";
        case vk::ObjectType::ePipelineLayout: return "PipelineLayout";
        case vk::ObjectType::eRenderPass: return "RenderPass";
        case vk::ObjectType::ePipeline: return "Pipeline";
        case vk::ObjectType::eDescriptorSetLayout: return "DescriptorSetLayout";
        case vk::ObjectType::eSampler: return "Sampler";
        case vk::ObjectType::eDescriptorPool: return "DescriptorPool";
        case vk::ObjectType::eDescriptorSet: return "DescriptorSet";
        case vk::ObjectType::eFramebuffer: return "Framebuffer";
        case vk::ObjectType::eCommandPool: return "CommandPool";
        case vk::ObjectType::eSamplerYcbcrConversion: return "SamplerYcbcrConversion";
        case vk::ObjectType::eDescriptorUpdateTemplate: return "DescriptorUpdateTemplate";
        case vk::ObjectType::eSurfaceKHR: return "SurfaceKHR";
        case vk::ObjectType::eSwapchainKHR: return "SwapchainKHR";
        case vk::ObjectType::eDisplayKHR: return "DisplayKHR";
        case vk::ObjectType::eDisplayModeKHR: return "DisplayModeKHR";
        case vk::ObjectType::eDebugReportCallbackEXT: return "DebugReportCallbackEXT";
        case vk::ObjectType::eObjectTableNVX: return "ObjectTableNVX";
        case vk::ObjectType::eIndirectCommandsLayoutNVX: return "IndirectCommandsLayoutNVX";
        case vk::ObjectType::eDebugUtilsMessengerEXT: return "DebugUtilsMessengerEXT";
        case vk::ObjectType::eValidationCacheEXT: return "ValidationCacheEXT";
        case vk::ObjectType::eAccelerationStructureNV: return "AccelerationStructureNV";
        case vk::ObjectType::ePerformanceConfigurationINTEL: return "PerformanceConfigurationINTEL";
        }

        return "Unknown";
    }

    static constexpr const char* to_string(const VkObjectType type)
    {
        // namespace needed because there is a `std::string to_string(vk::ObjectType type)`
        return my_app::to_string(static_cast<vk::ObjectType>(type));
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL
    DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                  VkDebugUtilsMessageTypeFlagsEXT message_type,
                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*)
    {
        std::string severity{};
        switch (message_severity)
        {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: severity = "[INFO]"; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: severity = "[ERROR]"; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: severity = "[VERBOSE]"; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: severity = "[WARNING]"; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT: break;
        }

        std::string type{};
        if (message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
            type += "[GENERAL]";
        if (message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
            type += "[VALIDATION]";
        if (message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
            type += "[PERFORMANCE]";

        std::cerr << severity << type << " " << pCallbackData->pMessage << "\n";

        if (pCallbackData->objectCount)
        {
            std::cerr << "Objects: \n";
            for (size_t i = 0; i < pCallbackData->objectCount; i++)
            {
                auto &object = pCallbackData->pObjects[i];
                std::cerr << "\t [" << i << "] "
                          << to_string(object.objectType)
                          << " "
                          << (object.pObjectName ? object.pObjectName : "NoName")
                          << std::endl;
            }
        }

        return VK_FALSE;
    }

    static std::vector<const char*> get_extensions()
    {
        std::vector<const char*> extensions;

        uint32_t required_count;
        const char** required_extensions = glfwGetRequiredInstanceExtensions(&required_count);
        for (size_t i = 0; i < required_count; i++)
            extensions.push_back(required_extensions[i]);

        if (ENABLE_VALIDATION_LAYERS)
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        return extensions;
    }

    static std::vector<const char*> get_layers()
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
        , command_pool(device->createCommandPoolUnique({ { vk::CommandPoolCreateFlagBits::eResetCommandBuffer }, static_cast<uint32_t>(graphics_family_idx) }))
        , texture_command_buffer(std::move(device->allocateCommandBuffersUnique({ command_pool.get(), vk::CommandBufferLevel::ePrimary, 1 })[0]))
    {
    }

    VulkanContext::~VulkanContext()
    {
        if (debug_messenger)
            instance->destroyDebugUtilsMessengerEXT(*debug_messenger, nullptr, dldi);

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
        create_info.enabledLayerCount = static_cast<uint32_t>(layers.size());
        create_info.ppEnabledLayerNames = layers.data();
        create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
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
        for (const auto& d : physical_devices)
        {
            auto properties = d.getProperties();
            std::cout << "Device: " << properties.deviceName << "\n";
            if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
            {
                std::cout << "Selected: " << properties.deviceName << "\n";
                selected = d;
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
        for (uint32_t i = 0; i < queue_families.size(); i++)
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
        dci.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
        dci.pQueueCreateInfos = queue_create_infos.data();
        dci.enabledLayerCount = static_cast<uint32_t>(layers.size());
        dci.ppEnabledLayerNames = layers.data();
        dci.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        dci.ppEnabledExtensionNames = extensions.data();
        dci.pEnabledFeatures = &features;

        return physical_device.createDeviceUnique(dci);
    }

    VmaAllocator VulkanContext::init_allocator() const
    {
        VmaAllocator result;
        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.physicalDevice = physical_device;
        allocatorInfo.device = *device;
        vmaCreateAllocator(&allocatorInfo, &result);
        return result;
    }

    void VulkanContext::transition_layout_cmd(vk::CommandBuffer cmd, vk::Image image, ThsvsAccessType prev_access, ThsvsAccessType next_access, vk::ImageSubresourceRange subresource_range) const
    {
        ThsvsImageBarrier image_barrier;
        image_barrier.prevAccessCount = 1;
        image_barrier.pPrevAccesses = &prev_access;
        image_barrier.nextAccessCount = 1;
        image_barrier.pNextAccesses = &next_access;
        image_barrier.prevLayout = THSVS_IMAGE_LAYOUT_OPTIMAL;
        image_barrier.nextLayout = THSVS_IMAGE_LAYOUT_OPTIMAL;
        image_barrier.discardContents = VK_FALSE;
        image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image_barrier.image = image;
        image_barrier.subresourceRange = subresource_range;

        thsvsCmdPipelineBarrier(
            cmd,
            nullptr,
            0,
            nullptr,
            1,
            &image_barrier);
    }

    void VulkanContext::transition_layout(vk::Image image, ThsvsAccessType prev_access, ThsvsAccessType next_access, vk::ImageSubresourceRange subresource_range) const
    {
        texture_command_buffer->begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

        transition_layout_cmd(*texture_command_buffer, image, prev_access, next_access, subresource_range);

        texture_command_buffer->end();

        vk::UniqueFence fence = device->createFenceUnique({});
        vk::SubmitInfo submit{};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &texture_command_buffer.get();

        get_graphics_queue().submit(submit, fence.get());
        device->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
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
        dslci.bindingCount = static_cast<uint32_t>(bindings.size());
        dslci.pBindings = bindings.data();
        return device->createDescriptorSetLayoutUnique(dslci);
    }

    void VulkanContext::submit_and_wait_cmd(vk::CommandBuffer cmd) const
    {
        // Submit
        vk::UniqueFence fence = device->createFenceUnique({});

        vk::SubmitInfo si{};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd;
        get_graphics_queue().submit({ si }, fence.get());
        device->waitForFences({ fence.get() }, VK_FALSE, UINT64_MAX);
    }

    Buffer VulkanContext::copy_data_to_image_cmd(vk::CommandBuffer cmd, CopyDataToImageParams params) const
    {
        // Create a staging buffer
        std::string name = "Staging buffer CopyDataToImage for size ";
        name += std::to_string(params.data_size);
        Buffer staging_buffer{ name, allocator, params.data_size, vk::BufferUsageFlagBits::eTransferSrc };
        void* mapped = staging_buffer.map();
        std::memcpy(mapped, params.data, params.data_size);

        // Set the image as a transfer destination
        transition_layout_cmd(cmd, params.target_image.get_image(), params.current_image_access, THSVS_ACCESS_TRANSFER_WRITE, params.subresource_range);

        std::vector<vk::BufferImageCopy> buffer_image_copy;
        buffer_image_copy.reserve(params.subresource_range.levelCount);
        for (uint32_t i = params.subresource_range.baseMipLevel; i < params.subresource_range.baseMipLevel + params.subresource_range.levelCount; ++i)
        {
            buffer_image_copy.emplace_back(
                0,                                             // VkDeviceSize                           bufferOffset
                0,                                             // uint32_t                               bufferRowLength
                0,                                             // uint32_t                               bufferImageHeight
                vk::ImageSubresourceLayers(                    // VkImageSubresourceLayers               imageSubresource
                    params.subresource_range.aspectMask,              // VkImageAspectFlags                     aspectMask
                    i,                                         // uint32_t                               mipLevel
                    params.subresource_range.baseArrayLayer,          // uint32_t                               baseArrayLayer
                    params.subresource_range.layerCount               // uint32_t                               layerCount
                    ),
                vk::Offset3D(),    // VkOffset3D                             imageOffset
                vk::Extent3D(      // VkExtent3D                             imageExtent
                    params.width,         // uint32_t                               width
                    params.height,        // uint32_t                               height
                    1              // uint32_t                               depth
                    ));
        }

        cmd.copyBufferToImage(staging_buffer.get_buffer(), params.target_image.get_image(), vk::ImageLayout::eTransferDstOptimal, buffer_image_copy);

        // Set the image to its new layout
        transition_layout_cmd(cmd, params.target_image.get_image(), THSVS_ACCESS_TRANSFER_WRITE, params.next_image_access, params.subresource_range);

        return staging_buffer;
    }

    void VulkanContext::copy_data_to_image(CopyDataToImageParams params) const
    {
        if (!params.data || !params.data_size)
            return;

        texture_command_buffer->begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
        auto staging_buffer = copy_data_to_image_cmd(texture_command_buffer.get(), params);
        texture_command_buffer->end();
        submit_and_wait_cmd(texture_command_buffer.get());

        staging_buffer.free();
    }

    Buffer VulkanContext::copy_data_to_buffer_cmd(vk::CommandBuffer cmd, CopyDataToBufferParams params) const
    {
        // Create staging buffer and map it's memory to copy data from the CPU
        std::string name = "Staging buffer CopyDataToBuffer for size ";
        name += std::to_string(params.data_size);
        Buffer staging_buffer{ name, allocator, params.data_size, vk::BufferUsageFlagBits::eTransferSrc };
        void* mapped = staging_buffer.map();
        std::memcpy(mapped, params.data, params.data_size);

        vk::BufferMemoryBarrier bmb_pre{};
        bmb_pre.srcAccessMask = params.current_buffer_access;
        bmb_pre.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        bmb_pre.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bmb_pre.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bmb_pre.buffer = params.buffer.get_buffer();
        bmb_pre.offset = 0;
        bmb_pre.size = static_cast<uint32_t>(params.data_size);
        cmd.pipelineBarrier(params.generating_stages, vk::PipelineStageFlagBits::eTransfer, {}, {}, { bmb_pre }, {});

        vk::BufferCopy bcp{};
        bcp.srcOffset = 0;
        bcp.dstOffset = 0;
        bcp.size = static_cast<uint32_t>(params.data_size);
        cmd.copyBuffer(staging_buffer.get_buffer(), params.buffer.get_buffer(), { bcp });

        vk::BufferMemoryBarrier bmb_post{};
        bmb_post.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        bmb_post.dstAccessMask = params.new_buffer_access;
        bmb_post.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bmb_post.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bmb_post.buffer = params.buffer.get_buffer();
        bmb_post.offset = 0;
        bmb_post.size = static_cast<uint32_t>(params.data_size);
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, params.consuming_stages, {}, {}, { bmb_post }, {});

        return staging_buffer;
    }

    void VulkanContext::copy_data_to_buffer(CopyDataToBufferParams params) const
    {
        if (!params.data || !params.data_size)
            return;

        // Record command buffer which copies data from the staging buffer to the destination buffer
        texture_command_buffer->begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
        auto staging_buffer = copy_data_to_buffer_cmd(texture_command_buffer.get(), params);
        texture_command_buffer->end();
        submit_and_wait_cmd(texture_command_buffer.get());

        staging_buffer.free();
    }

    void VulkanContext::clear_buffer_cmd(vk::CommandBuffer cmd, const Buffer& buffer, uint32_t data) const
    {
        cmd.fillBuffer(buffer.get_buffer(), 0, buffer.get_size(), data);
    }

    void VulkanContext::clear_buffer(const Buffer& buffer, uint32_t data) const
    {
        texture_command_buffer->begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

        texture_command_buffer->fillBuffer(buffer.get_buffer(), 0, buffer.get_size(), data);

        texture_command_buffer->end();
        submit_and_wait_cmd(texture_command_buffer.get());
    }
}    // namespace my_app
