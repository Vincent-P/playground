#include <vulkan/vulkan.hpp>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vk_mem_alloc.h>

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>

#include "buffer.hpp"
#include "image.hpp"
#include "model.hpp"

namespace my_app
{
    constexpr int WIDTH = 800;
    constexpr int HEIGHT = 600;
    constexpr int NUM_FRAME_DATA = 2;

    constexpr bool enableValidationLayers = true;
    constexpr std::array<const char*, 1> g_validation_layers = {
        "VK_LAYER_LUNARG_standard_validation"};
    constexpr std::array<const char*, 1> g_device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    struct UniformBufferObject
    {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };

    std::vector<char> readFile(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            throw std::runtime_error("failed to open file!");
        }

        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    }

    struct VulkanContext
    {
        vk::UniqueInstance instance;
        vk::DebugUtilsMessengerEXT debug_messenger;
        vk::DispatchLoaderDynamic dldi;

        VmaAllocator allocator;
        vk::SurfaceKHR surface;

        vk::PhysicalDevice physical_device;
        vk::Device device;
        vk::DispatchLoaderDynamic dldid;

        size_t graphics_family_idx;
        size_t present_family_idx;

        vk::UniqueSwapchainKHR swapchain;
        std::vector<vk::Image> swapchain_images;
        std::vector<vk::ImageView> swapchain_image_views;
        vk::Format swapchain_format;
        vk::PresentModeKHR swapchain_present_mode;
        vk::Extent2D swapchain_extent;

        vk::CommandPool command_pool;
        std::vector<vk::CommandBuffer> command_buffers;
        std::vector<vk::Fence> command_buffers_fences;
        std::vector<vk::Semaphore> acquire_semaphores;
        std::vector<vk::Semaphore> render_complete_semaphores;

        Image depth_image;
        vk::ImageView depth_image_view;
        vk::Format depth_format;

        Buffer uniform_buffer;
        Buffer index_buffer;
        Buffer vertex_buffer;

        vk::ShaderModule vert_module;
        vk::ShaderModule frag_module;

        vk::DescriptorPool desc_pool;
        std::vector<vk::DescriptorSet> desc_sets;

        std::vector<vk::PipelineShaderStageCreateInfo> shader_stages;

        vk::Pipeline pipeline;
        vk::PipelineCache pipeline_cache;
        vk::PipelineLayout pipeline_layout;
        vk::RenderPass render_pass;
        std::vector<vk::Framebuffer> frame_buffers;
    };

    static VKAPI_ATTR VkBool32 VKAPI_CALL
    DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                  VkDebugUtilsMessageTypeFlagsEXT messageType,
                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
    {

        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }

    class Application
    {
        public:
        Application()
            : model_("models/Box/glTF/Box.gltf")
        {
            CreateGLFWWindow();
            CreateInstance();
            SetupDebugCallback();
            CreateSurface();
            CreateLogicalDevice();
            InitAllocator();
            CreateSwapchain();
            CreateCommandPoolAndBuffers();
            CreateSemaphores();
            CreateDepthBuffer();
            CreateUniformBuffer();
            CreateDescriptors();
            CreateRenderPass();
            CreateFrameBuffers();
            CreateVertexBuffer();
            CreateIndexBuffer();
            LoadShaders();
            CreateGraphicsPipeline();
            FillCommandBuffers();
        }

        ~Application()
        {

            if (enableValidationLayers)
                vk_ctx_.instance->destroyDebugUtilsMessengerEXT(vk_ctx_.debug_messenger,
                                                                nullptr,
                                                                vk_ctx_.dldi);
            vk_ctx_.vertex_buffer.Free();
            vk_ctx_.index_buffer.Free();
            vk_ctx_.uniform_buffer.Free();
            vk_ctx_.depth_image.Free();

            vmaDestroyAllocator(vk_ctx_.allocator);
            glfwDestroyWindow(window_);
            glfwTerminate();
        }

        void CreateGLFWWindow()
        {
            glfwInit();

            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

            window_ = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
        }

        std::vector<const char*> GetExtensions()
        {
            auto installed = vk::enumerateInstanceExtensionProperties();

            std::vector<const char*> extensions;

            uint32_t required_count;
            const char** required_extensions = glfwGetRequiredInstanceExtensions(&required_count);
            for (auto i = 0; i < required_count; i++)
                extensions.push_back(required_extensions[i]);

            if (enableValidationLayers)
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

        void CreateInstance()
        {
            vk::ApplicationInfo app_info;
            app_info.pApplicationName = "MyApp";
            app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            app_info.pEngineName = "MyEngine";
            app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0),
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

            vk_ctx_.instance = vk::createInstanceUnique(create_info);
            vk_ctx_.dldi = vk::DispatchLoaderDynamic(*vk_ctx_.instance);
        }

        void SetupDebugCallback()
        {
            if (!enableValidationLayers)
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

            vk_ctx_.debug_messenger = vk_ctx_.instance->createDebugUtilsMessengerEXT(ci, nullptr, vk_ctx_.dldi);
        }

        void CreateSurface()
        {
            if (glfwCreateWindowSurface(static_cast<VkInstance>(*vk_ctx_.instance), window_, nullptr, reinterpret_cast<VkSurfaceKHR*>(&vk_ctx_.surface)) != VK_SUCCESS)
                throw std::runtime_error("failed to create window surface.");
        }

        void CreateLogicalDevice()
        {
            auto physical_devices = vk_ctx_.instance->enumeratePhysicalDevices();
            auto gpu = *std::find_if(physical_devices.begin(), physical_devices.end(), [](const vk::PhysicalDevice& d) -> auto {
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
                    vk_ctx_.graphics_family_idx = i;
                }

                if (!has_present && gpu.getSurfaceSupportKHR(i, vk_ctx_.surface))
                {
                    // Create a single graphics queue.
                    queue_create_infos.push_back(
                        vk::DeviceQueueCreateInfo(vk::DeviceQueueCreateFlags(), i, 1, &priority));
                    has_present = true;
                    vk_ctx_.present_family_idx = i;
                }
            }

            if (!has_present || !has_graphics)
                throw std::runtime_error("failed to find a graphics and present queue.");

            if (vk_ctx_.present_family_idx == vk_ctx_.graphics_family_idx)
            {
                queue_create_infos.pop_back();
            }

            vk_ctx_.physical_device = gpu;

            vk::DeviceCreateInfo dci;
            dci.flags = {};
            dci.queueCreateInfoCount = queue_create_infos.size();
            dci.pQueueCreateInfos = queue_create_infos.data();
            dci.enabledLayerCount = layers.size();
            dci.ppEnabledLayerNames = layers.data();
            dci.enabledExtensionCount = extensions.size();
            dci.ppEnabledExtensionNames = extensions.data();
            dci.pEnabledFeatures = &features;

            vk_ctx_.device = gpu.createDevice(dci);
        }

        void InitAllocator()
        {
            VmaAllocatorCreateInfo allocatorInfo = {};
            allocatorInfo.physicalDevice = vk_ctx_.physical_device;
            allocatorInfo.device = vk_ctx_.device;
            vmaCreateAllocator(&allocatorInfo, &vk_ctx_.allocator);
        }

        void CreateSwapchain()
        {
            auto capabilities = vk_ctx_.physical_device.getSurfaceCapabilitiesKHR(vk_ctx_.surface);
            auto extent = capabilities.currentExtent;

            auto present_modes = vk_ctx_.physical_device.getSurfacePresentModesKHR(vk_ctx_.surface);
            auto present_mode = vk::PresentModeKHR::eFifo;
            for (auto& pm : present_modes)
            {
                if (pm == vk::PresentModeKHR::eMailbox)
                {
                    present_mode = vk::PresentModeKHR::eMailbox;
                    break;
                }
            }
            if (present_mode == vk::PresentModeKHR::eFifo)
            {
                for (auto& pm : present_modes)
                {
                    if (pm == vk::PresentModeKHR::eImmediate)
                    {
                        present_mode = vk::PresentModeKHR::eImmediate;
                        break;
                    }
                }
            }

            auto formats = vk_ctx_.physical_device.getSurfaceFormatsKHR(vk_ctx_.surface);
            auto format = formats[0];
            if (format.format == vk::Format::eUndefined)
            {
                format.format = vk::Format::eB8G8R8A8Unorm;
                format.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
            }
            else
            {
                for (const auto& f : formats)
                {
                    if (f.format == vk::Format::eB8G8R8A8Unorm &&
                        f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
                    {
                        format = f;
                        break;
                    }
                }
            }

            assert(capabilities.maxImageCount >= NUM_FRAME_DATA);
            auto ci = vk::SwapchainCreateInfoKHR();
            ci.surface = vk_ctx_.surface;
            ci.minImageCount = NUM_FRAME_DATA;
            ci.imageFormat = format.format;
            ci.imageColorSpace = format.colorSpace;
            ci.imageExtent = extent;
            ci.imageArrayLayers = 1;
            ci.imageUsage =
                vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc;

            if (vk_ctx_.graphics_family_idx != vk_ctx_.present_family_idx)
            {
                uint32_t indices[] = {(uint32_t)vk_ctx_.graphics_family_idx,
                                      (uint32_t)vk_ctx_.present_family_idx};
                ci.imageSharingMode = vk::SharingMode::eConcurrent;
                ci.queueFamilyIndexCount = 2;
                ci.pQueueFamilyIndices = indices;
            }
            else
            {
                ci.imageSharingMode = vk::SharingMode::eExclusive;
            }

            ci.preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
            ci.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
            ci.presentMode = present_mode;
            ci.clipped = VK_TRUE;

            vk_ctx_.swapchain = vk_ctx_.device.createSwapchainKHRUnique(ci);
            vk_ctx_.swapchain_images = vk_ctx_.device.getSwapchainImagesKHR(*vk_ctx_.swapchain);
            vk_ctx_.swapchain_format = format.format;
            vk_ctx_.swapchain_present_mode = present_mode;
            vk_ctx_.swapchain_extent = extent;

            vk_ctx_.swapchain_image_views.clear();
            for (const auto& image : vk_ctx_.swapchain_images)
            {
                auto ci = vk::ImageViewCreateInfo();
                ci.image = image;
                ci.viewType = vk::ImageViewType::e2D;
                ci.format = vk_ctx_.swapchain_format;
                ci.components.r = vk::ComponentSwizzle::eR;
                ci.components.g = vk::ComponentSwizzle::eG;
                ci.components.b = vk::ComponentSwizzle::eB;
                ci.components.a = vk::ComponentSwizzle::eA;
                ci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
                ci.subresourceRange.baseMipLevel = 0;
                ci.subresourceRange.levelCount = 1;
                ci.subresourceRange.baseArrayLayer = 0;
                ci.subresourceRange.layerCount = 1;
                ci.flags = {};
                auto view = vk_ctx_.device.createImageView(ci);
                vk_ctx_.swapchain_image_views.push_back(view);
            }
        }

        void CreateCommandPoolAndBuffers()
        {
            vk::CommandPoolCreateInfo ci;
            ci.flags = vk::CommandPoolCreateFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
            ci.queueFamilyIndex = vk_ctx_.graphics_family_idx;

            vk_ctx_.command_pool = vk_ctx_.device.createCommandPool(ci);

            vk::CommandBufferAllocateInfo cbai;
            cbai.commandPool = vk_ctx_.command_pool;
            cbai.level = vk::CommandBufferLevel::ePrimary;
            cbai.commandBufferCount = NUM_FRAME_DATA;

            vk_ctx_.command_buffers = vk_ctx_.device.allocateCommandBuffers(cbai);

            vk_ctx_.command_buffers_fences.resize(vk_ctx_.command_buffers.size());
            for (int i = 0; i < vk_ctx_.command_buffers.size(); i++)
            {
                vk_ctx_.command_buffers_fences[i] = vk_ctx_.device.createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
            }
        }

        void CreateSemaphores()
        {
            vk_ctx_.acquire_semaphores.resize(NUM_FRAME_DATA);
            vk_ctx_.render_complete_semaphores.resize(NUM_FRAME_DATA);

            for (int i = 0; i < NUM_FRAME_DATA; ++i)
            {
                vk_ctx_.acquire_semaphores[i] =
                    vk_ctx_.device.createSemaphore(vk::SemaphoreCreateInfo());
                vk_ctx_.render_complete_semaphores[i] =
                    vk_ctx_.device.createSemaphore(vk::SemaphoreCreateInfo());
            }
        }

        void CreateDepthBuffer()
        {
            std::vector<vk::Format> depthFormats = {
                vk::Format::eD32SfloatS8Uint, vk::Format::eD32Sfloat, vk::Format::eD24UnormS8Uint,
                vk::Format::eD16UnormS8Uint, vk::Format::eD16Unorm};

            for (auto& format : depthFormats)
            {
                auto depthFormatProperties = vk_ctx_.physical_device.getFormatProperties(format);
                // Format must support depth stencil attachment for optimal tiling
                if (depthFormatProperties.optimalTilingFeatures &
                    vk::FormatFeatureFlagBits::eDepthStencilAttachment)
                {
                    vk_ctx_.depth_format = format;
                    break;
                }
            }

            auto ci = vk::ImageCreateInfo();
            ci.imageType = vk::ImageType::e2D;
            ci.format = vk_ctx_.depth_format;
            ci.extent.width = vk_ctx_.swapchain_extent.width;
            ci.extent.height = vk_ctx_.swapchain_extent.height;
            ci.extent.depth = 1;
            ci.mipLevels = 1;
            ci.arrayLayers = 1;
            ci.samples = vk::SampleCountFlagBits::e1;
            ci.initialLayout = vk::ImageLayout::eUndefined;
            ci.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
            ci.queueFamilyIndexCount = 0;
            ci.pQueueFamilyIndices = NULL;
            ci.sharingMode = vk::SharingMode::eExclusive;
            ci.flags = {};

            vk_ctx_.depth_image = Image(ci, VMA_MEMORY_USAGE_GPU_ONLY, vk_ctx_.allocator);

            auto vci = vk::ImageViewCreateInfo();
            vci.flags = {};
            vci.format = vk_ctx_.depth_format;
            vci.image = vk_ctx_.depth_image.GetImage();
            vci.viewType = vk::ImageViewType::e2D;
            vci.subresourceRange.aspectMask =
                vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
            vci.subresourceRange.baseMipLevel = 0;
            vci.subresourceRange.levelCount = 1;
            vci.subresourceRange.baseArrayLayer = 0;
            vci.subresourceRange.layerCount = 1;
            vk_ctx_.depth_image_view = vk_ctx_.device.createImageView(vci);
        }

        void CreateUniformBuffer()
        {
            // transformation, angle, rotations axis
            ubo_.model =
                glm::rotate(glm::mat4(1.0f), glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

            ubo_.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                                    glm::vec3(0.0f, 0.0f, 1.0f));

            ubo_.proj = glm::perspective(glm::radians(45.0f),
                                         vk_ctx_.swapchain_extent.width /
                                             (float)vk_ctx_.swapchain_extent.height,
                                         0.1f, 10.0f);

            ubo_.proj[1][1] *= -1;

            auto buf_usage = vk::BufferUsageFlagBits::eUniformBuffer;
            auto mem_usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

            vk_ctx_.uniform_buffer = Buffer(sizeof(ubo_), buf_usage, mem_usage, vk_ctx_.allocator);

            void* mappedData = vk_ctx_.uniform_buffer.Map();
            memcpy(mappedData, &ubo_, sizeof(ubo_));
            vk_ctx_.uniform_buffer.Unmap();
        }

        void CreateDescriptors()
        {
            std::vector<vk::DescriptorPoolSize> descriptorPoolSizes = {
                vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1)};

            vk_ctx_.desc_pool = vk_ctx_.device.createDescriptorPool(
                vk::DescriptorPoolCreateInfo(
                    vk::DescriptorPoolCreateFlags(),
                    1,
                    descriptorPoolSizes.size(),
                    descriptorPoolSizes.data()));

            // Binding 0: Uniform buffer (Vertex shader)
            std::vector<vk::DescriptorSetLayoutBinding> descriptorSetLayoutBindings =
                {
                    vk::DescriptorSetLayoutBinding(
                        0,
                        vk::DescriptorType::eUniformBuffer,
                        1,
                        vk::ShaderStageFlagBits::eVertex,
                        nullptr)};

            std::vector<vk::DescriptorSetLayout> descriptorSetLayouts = {
                vk_ctx_.device.createDescriptorSetLayout(
                    vk::DescriptorSetLayoutCreateInfo(
                        vk::DescriptorSetLayoutCreateFlags(),
                        descriptorSetLayoutBindings.size(),
                        descriptorSetLayoutBindings.data()))};

            vk_ctx_.desc_sets = vk_ctx_.device.allocateDescriptorSets(
                vk::DescriptorSetAllocateInfo(
                    vk_ctx_.desc_pool,
                    descriptorSetLayouts.size(),
                    descriptorSetLayouts.data()));

            auto ci = vk::PipelineLayoutCreateInfo();
            ci.pSetLayouts = descriptorSetLayouts.data();
            ci.setLayoutCount = descriptorSetLayouts.size();
            vk_ctx_.pipeline_layout = vk_ctx_.device.createPipelineLayout(ci);

            auto dbi = vk::DescriptorBufferInfo();
            dbi.buffer = vk_ctx_.uniform_buffer.GetBuffer();
            dbi.offset = 0;
            dbi.range = sizeof(ubo_);

            std::array<vk::WriteDescriptorSet, 1> writes;
            writes[0].setDstSet(vk_ctx_.desc_sets[0]);
            writes[0].setDescriptorCount(vk_ctx_.desc_sets.size());
            writes[0].setDescriptorType(vk::DescriptorType::eUniformBuffer);
            writes[0].pBufferInfo = &dbi;
            writes[0].dstArrayElement = 0;
            writes[0].dstBinding = 0;

            vk_ctx_.device.updateDescriptorSets(writes, nullptr);
        }

        void CreateRenderPass()
        {
            std::array<vk::AttachmentDescription, 2> attachments;

            attachments[0].format = vk_ctx_.swapchain_format;
            attachments[0].samples = vk::SampleCountFlagBits::e1;
            attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
            attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
            attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            attachments[0].initialLayout = vk::ImageLayout::eUndefined;
            attachments[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;
            attachments[0].flags = vk::AttachmentDescriptionFlags();

            attachments[1].format = vk_ctx_.depth_format;
            attachments[1].samples = vk::SampleCountFlagBits::e1;
            attachments[1].loadOp = vk::AttachmentLoadOp::eClear;
            attachments[1].storeOp = vk::AttachmentStoreOp::eDontCare;
            attachments[1].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            attachments[1].initialLayout = vk::ImageLayout::eUndefined;
            attachments[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
            attachments[1].flags = vk::AttachmentDescriptionFlags();

            auto color_ref = vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal);
            auto depth_ref =
                vk::AttachmentReference(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);

            vk::SubpassDescription subpass = {};
            subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
            subpass.flags = vk::SubpassDescriptionFlags();
            subpass.inputAttachmentCount = 0;
            subpass.pInputAttachments = nullptr;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &color_ref;
            subpass.pResolveAttachments = nullptr;
            subpass.pDepthStencilAttachment = &depth_ref;
            subpass.preserveAttachmentCount = 0;
            subpass.pPreserveAttachments = nullptr;

            vk::RenderPassCreateInfo rp_info = {};
            rp_info.attachmentCount = attachments.size();
            rp_info.pAttachments = attachments.data();
            rp_info.subpassCount = 1;
            rp_info.pSubpasses = &subpass;
            rp_info.dependencyCount = 0;
            rp_info.pDependencies = nullptr;
            vk_ctx_.render_pass = vk_ctx_.device.createRenderPass(rp_info);
        }

        void CreateFrameBuffers()
        {
            std::array<vk::ImageView, 2> attachments;
            attachments[1] = vk_ctx_.depth_image_view;

            vk_ctx_.frame_buffers.resize(vk_ctx_.swapchain_images.size());

            vk::FramebufferCreateInfo ci;
            ci.renderPass = vk_ctx_.render_pass;
            ci.attachmentCount = attachments.size();
            ci.pAttachments = attachments.data();
            ci.width = vk_ctx_.swapchain_extent.width;
            ci.height = vk_ctx_.swapchain_extent.height;
            ci.layers = 1;

            for (size_t i = 0; i < vk_ctx_.swapchain_image_views.size(); i++)
            {
                attachments[0] = vk_ctx_.swapchain_image_views[i];
                vk_ctx_.frame_buffers[i] = vk_ctx_.device.createFramebuffer(ci);
            }
        }

        void CreateIndexBuffer()
        {
            auto size = model_.indices_.size() * sizeof(std::uint32_t);
            auto buf_usage = vk::BufferUsageFlagBits::eIndexBuffer;
            auto mem_usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

            vk_ctx_.index_buffer = Buffer(size, buf_usage, mem_usage, vk_ctx_.allocator);

            void* mappedData = vk_ctx_.index_buffer.Map();
            memcpy(mappedData, model_.indices_.data(), size);
            vk_ctx_.index_buffer.Unmap();
        }

        void CreateVertexBuffer()
        {
            auto size = model_.vertices_.size() * sizeof(Vertex);
            auto buf_usage = vk::BufferUsageFlagBits::eVertexBuffer;
            auto mem_usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

            vk_ctx_.vertex_buffer = Buffer(size, buf_usage, mem_usage, vk_ctx_.allocator);

            void* mappedData = vk_ctx_.vertex_buffer.Map();
            memcpy(mappedData, model_.vertices_.data(), size);
            vk_ctx_.vertex_buffer.Unmap();
        }

        void LoadShaders()
        {
            std::vector<char> vert_code = readFile("shaders/vert.spv");
            std::vector<char> frag_code = readFile("shaders/frag.spv");

            auto vert_i = vk::ShaderModuleCreateInfo();
            vert_i.codeSize = vert_code.size();
            vert_i.pCode = reinterpret_cast<const uint32_t*>(vert_code.data());
            vk_ctx_.vert_module = vk_ctx_.device.createShaderModule(vert_i);

            auto frag_i = vk::ShaderModuleCreateInfo();
            frag_i.codeSize = frag_code.size();
            frag_i.pCode = reinterpret_cast<const uint32_t*>(frag_code.data());
            vk_ctx_.frag_module = vk_ctx_.device.createShaderModule(frag_i);

            std::vector<vk::PipelineShaderStageCreateInfo> pipelineShaderStages = {
                vk::PipelineShaderStageCreateInfo(
                    vk::PipelineShaderStageCreateFlags(),
                    vk::ShaderStageFlagBits::eVertex,
                    vk_ctx_.vert_module,
                    "main",
                    nullptr),

                vk::PipelineShaderStageCreateInfo(
                    vk::PipelineShaderStageCreateFlags(),
                    vk::ShaderStageFlagBits::eFragment,
                    vk_ctx_.frag_module,
                    "main",
                    nullptr)};

            vk_ctx_.shader_stages = pipelineShaderStages;
        }

        void CreateGraphicsPipeline()
        {
            std::vector<vk::DynamicState> dynamic_states =
                {
                    vk::DynamicState::eViewport,
                    vk::DynamicState::eScissor};

            vk::PipelineDynamicStateCreateInfo dyn_i(
                vk::PipelineDynamicStateCreateFlags(),
                dynamic_states.size(),
                dynamic_states.data());

            auto bindings = Vertex::getBindingDescriptions();
            auto attributes = Vertex::getAttributeDescriptions();
            vk::PipelineVertexInputStateCreateInfo vert_i;
            vert_i.flags = vk::PipelineVertexInputStateCreateFlags();
            vert_i.vertexBindingDescriptionCount = bindings.size();
            vert_i.pVertexBindingDescriptions = bindings.data();
            vert_i.vertexAttributeDescriptionCount = attributes.size();
            vert_i.pVertexAttributeDescriptions = attributes.data();

            vk::PipelineInputAssemblyStateCreateInfo asm_i;
            asm_i.flags = vk::PipelineInputAssemblyStateCreateFlags();
            asm_i.primitiveRestartEnable = VK_FALSE;
            asm_i.topology = vk::PrimitiveTopology::eTriangleList;

            vk::PipelineRasterizationStateCreateInfo rast_i;
            rast_i.flags = vk::PipelineRasterizationStateCreateFlags();
            rast_i.polygonMode = vk::PolygonMode::eFill;
            rast_i.cullMode = vk::CullModeFlagBits::eBack;
            rast_i.frontFace = vk::FrontFace::eCounterClockwise;
            rast_i.depthClampEnable = VK_TRUE;
            rast_i.rasterizerDiscardEnable = VK_FALSE;
            rast_i.depthBiasEnable = VK_FALSE;
            rast_i.depthBiasConstantFactor = 0;
            rast_i.depthBiasClamp = 0;
            rast_i.depthBiasSlopeFactor = 0;
            rast_i.lineWidth = 1.0f;

            std::array<vk::PipelineColorBlendAttachmentState, 1> att_states;
            att_states[0].colorWriteMask = vk::ColorComponentFlags(
                vk::ColorComponentFlagBits::eR |
                vk::ColorComponentFlagBits::eG |
                vk::ColorComponentFlagBits::eB |
                vk::ColorComponentFlagBits::eA);
            att_states[0].blendEnable = VK_TRUE;
            att_states[0].srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
            att_states[0].dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
            att_states[0].colorBlendOp = vk::BlendOp::eAdd;
            att_states[0].srcAlphaBlendFactor = vk::BlendFactor::eOne;
            att_states[0].dstAlphaBlendFactor = vk::BlendFactor::eZero;
            att_states[0].alphaBlendOp = vk::BlendOp::eAdd;

            vk::PipelineColorBlendStateCreateInfo colorblend_i;
            colorblend_i.flags = vk::PipelineColorBlendStateCreateFlags();
            colorblend_i.attachmentCount = att_states.size();
            colorblend_i.pAttachments = att_states.data();
            colorblend_i.logicOpEnable = VK_FALSE;
            colorblend_i.logicOp = vk::LogicOp::eCopy;
            colorblend_i.blendConstants[0] = 0.0f;
            colorblend_i.blendConstants[1] = 0.0f;
            colorblend_i.blendConstants[2] = 0.0f;
            colorblend_i.blendConstants[3] = 0.0f;

            vk::PipelineViewportStateCreateInfo vp_i;
            vp_i.flags = vk::PipelineViewportStateCreateFlags();
            vp_i.viewportCount = 1;
            vp_i.scissorCount = 1;
            vp_i.pScissors = nullptr;
            vp_i.pViewports = nullptr;

            vk::PipelineDepthStencilStateCreateInfo ds_i;
            ds_i.flags = vk::PipelineDepthStencilStateCreateFlags();
            ds_i.depthTestEnable = VK_TRUE;
            ds_i.depthWriteEnable = VK_TRUE;
            ds_i.depthCompareOp = vk::CompareOp::eLessOrEqual;
            ds_i.depthBoundsTestEnable = VK_FALSE;
            ds_i.minDepthBounds = 0;
            ds_i.maxDepthBounds = 0;
            ds_i.stencilTestEnable = VK_FALSE;
            ds_i.back.failOp = vk::StencilOp::eKeep;
            ds_i.back.passOp = vk::StencilOp::eKeep;
            ds_i.back.compareOp = vk::CompareOp::eAlways;
            ds_i.back.compareMask = 0;
            ds_i.back.reference = 0;
            ds_i.back.depthFailOp = vk::StencilOp::eKeep;
            ds_i.back.writeMask = 0;
            ds_i.front = ds_i.back;

            vk::PipelineMultisampleStateCreateInfo ms_i;
            ms_i.flags = vk::PipelineMultisampleStateCreateFlags();
            ms_i.pSampleMask = nullptr;
            ms_i.rasterizationSamples = vk::SampleCountFlagBits::e1;
            ms_i.sampleShadingEnable = VK_FALSE;
            ms_i.alphaToCoverageEnable = VK_FALSE;
            ms_i.alphaToOneEnable = VK_FALSE;
            ms_i.minSampleShading = 0.0;

            vk::GraphicsPipelineCreateInfo pipe_i;
            pipe_i.layout = vk_ctx_.pipeline_layout;
            pipe_i.basePipelineHandle = nullptr;
            pipe_i.basePipelineIndex = 0;
            pipe_i.pVertexInputState = &vert_i;
            pipe_i.pInputAssemblyState = &asm_i;
            pipe_i.pRasterizationState = &rast_i;
            pipe_i.pColorBlendState = &colorblend_i;
            pipe_i.pTessellationState = nullptr;
            pipe_i.pMultisampleState = &ms_i;
            pipe_i.pDynamicState = &dyn_i;
            pipe_i.pViewportState = &vp_i;
            pipe_i.pDepthStencilState = &ds_i;
            pipe_i.pStages = vk_ctx_.shader_stages.data();
            pipe_i.stageCount = vk_ctx_.shader_stages.size();
            pipe_i.renderPass = vk_ctx_.render_pass;
            pipe_i.subpass = 0;

            vk_ctx_.pipeline_cache = vk_ctx_.device.createPipelineCache(vk::PipelineCacheCreateInfo());
            vk_ctx_.pipeline = vk_ctx_.device.createGraphicsPipeline(vk_ctx_.pipeline_cache, pipe_i);
        }

        void FillCommandBuffers()
        {
            auto renderArea = vk::Rect2D(vk::Offset2D(), vk_ctx_.swapchain_extent);

            std::array<vk::ClearValue, 2> clearValues = {};
            clearValues[0].color = vk::ClearColorValue(std::array<float, 4>{0.0f, 1.0f, 0.0f, 0.5f});
            clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

            // From here we can do common GL commands
            // Lets add commands to each command buffer.
            for (int32_t i = 0; i < vk_ctx_.command_buffers.size(); ++i)
            {
                vk_ctx_.command_buffers[i].begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eSimultaneousUse));

                vk_ctx_.command_buffers[i].beginRenderPass(
                    vk::RenderPassBeginInfo(
                        vk_ctx_.render_pass,
                        vk_ctx_.frame_buffers[i],
                        renderArea,
                        clearValues.size(),
                        clearValues.data()),
                    vk::SubpassContents::eInline);

                std::vector<vk::Viewport> viewports =
                    {
                        vk::Viewport(
                            0,
                            0,
                            vk_ctx_.swapchain_extent.width,
                            vk_ctx_.swapchain_extent.height,
                            0,
                            1.0f)};

                vk_ctx_.command_buffers[i].setViewport(0, viewports);

                std::vector<vk::Rect2D> scissors = {renderArea};

                vk_ctx_.command_buffers[i].setScissor(0, scissors);

                // Bind Descriptor Sets, these are attribute/uniform "descriptions"
                vk_ctx_.command_buffers[i].bindPipeline(
                    vk::PipelineBindPoint::eGraphics,
                    vk_ctx_.pipeline);

                vk_ctx_.command_buffers[i].bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,
                    vk_ctx_.pipeline_layout,
                    0,
                    vk_ctx_.desc_sets,
                    nullptr);

                vk::Buffer vertexBuffers[] = {vk_ctx_.vertex_buffer.GetBuffer()};
                vk::DeviceSize offsets[] = {0};
                vk_ctx_.command_buffers[i].bindIndexBuffer(
                    vk_ctx_.index_buffer.GetBuffer(),
                    0,
                    vk::IndexType::eUint32);

                vk_ctx_.command_buffers[i].bindVertexBuffers(0, 1, vertexBuffers, offsets);

                model_.draw(vk_ctx_.command_buffers[i]);

                vk_ctx_.command_buffers[i].endRenderPass();
                vk_ctx_.command_buffers[i].end();
            }
        }

        void run()
        {
            uint64_t frameCounter = 0;
            double frameTimer = 0.0;
            double fpsTimer = 0.0;
            double lastFPS = 0.0;

            auto& device = vk_ctx_.device;
            auto graphicsQueue = device.getQueue(vk_ctx_.graphics_family_idx, 0);
            uint32_t currentBuffer = 0;
            uint32_t imageIndex = 0;

            while (!glfwWindowShouldClose(window_))
            {
                glfwPollEvents();

                auto tStart = std::chrono::high_resolution_clock::now();

                device.waitForFences(1,
                                     &vk_ctx_.command_buffers_fences[currentBuffer],
                                     VK_TRUE,
                                     UINT64_MAX);

                device.resetFences(1, &vk_ctx_.command_buffers_fences[currentBuffer]);

                // will signal acquire_semaphore when the next image is acquired
                // and put its index in imageIndex
                device.acquireNextImageKHR(*vk_ctx_.swapchain,
                                           std::numeric_limits<uint64_t>::max(),
                                           vk_ctx_.acquire_semaphores[currentBuffer],
                                           nullptr,
                                           &imageIndex);

                // Create kernels to submit to the queue on a given render pass.
                vk::PipelineStageFlags stages[] = {
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                };

                auto kernel = vk::SubmitInfo();
                kernel.waitSemaphoreCount = 1;
                kernel.pWaitSemaphores = &vk_ctx_.acquire_semaphores[currentBuffer];
                kernel.pWaitDstStageMask = stages;
                kernel.commandBufferCount = 1;
                kernel.pCommandBuffers = &vk_ctx_.command_buffers[imageIndex];
                kernel.signalSemaphoreCount = 1;
                kernel.pSignalSemaphores = &vk_ctx_.render_complete_semaphores[currentBuffer];

                // the fences will be signaled
                graphicsQueue.submit(1, &kernel, vk_ctx_.command_buffers_fences[currentBuffer]);

                auto present_i = vk::PresentInfoKHR();
                present_i.waitSemaphoreCount = 1;
                present_i.pWaitSemaphores = &vk_ctx_.render_complete_semaphores[currentBuffer];
                present_i.swapchainCount = 1;
                present_i.pSwapchains = &vk_ctx_.swapchain.get();
                present_i.pImageIndices = &imageIndex;
                graphicsQueue.presentKHR(present_i);

                currentBuffer = (currentBuffer + 1) % NUM_FRAME_DATA;

                frameCounter++;
                auto tEnd = std::chrono::high_resolution_clock::now();
                auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
                frameTimer = tDiff / 1000.0;

                fpsTimer += tDiff;
                if (fpsTimer > 1000.0)
                {
                    std::string windowTitle = "Test vulkan - " + std::to_string(frameCounter) + " fps";
                    glfwSetWindowTitle(window_, windowTitle.c_str());

                    lastFPS = roundf(1.0 / frameTimer);
                    fpsTimer = 0.0;
                    frameCounter = 0;
                }
            }
            device.waitIdle();
        }

        private:
        Model model_;
        VulkanContext vk_ctx_;
        GLFWwindow* window_;
        UniformBufferObject ubo_;
    };
}    // namespace my_app

int main()
{
    my_app::Application app;
    app.run();

    return 0;
}
