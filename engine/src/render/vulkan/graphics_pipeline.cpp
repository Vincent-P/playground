#include "render/vulkan/descriptor_set.hpp"
#include "render/vulkan/resources.hpp"
#include "render/vulkan/device.hpp"
#include "render/vulkan/utils.hpp"
#include "vulkan/vulkan_core.h"

namespace vulkan
{

/// --- Renderpass

Handle<RenderPass> Device::create_renderpass(const RenderAttachments &render_attachments)
{
    Vec<VkAttachmentReference> color_refs;
    color_refs.reserve(render_attachments.colors.size());

    Vec<VkAttachmentDescription> attachment_descriptions;
    attachment_descriptions.reserve(render_attachments.colors.size() + 1);

    for (const auto &color : render_attachments.colors)
    {
        color_refs.push_back({
                .attachment = static_cast<u32>(attachment_descriptions.size()),
                .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            });

        attachment_descriptions.push_back(VkAttachmentDescription{
                .format = color.format,
                .samples = color.samples,
                .loadOp = color.load_op,
                .storeOp = color.store_op,
                .initialLayout = color.load_op == VK_ATTACHMENT_LOAD_OP_CLEAR ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            });
    }

    VkAttachmentReference depth_ref{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    if (render_attachments.depth)
    {
        depth_ref.attachment = static_cast<u32>(attachment_descriptions.size());

        attachment_descriptions.push_back(VkAttachmentDescription{
                .format = render_attachments.depth->format,
                .samples = render_attachments.depth->samples,
                .loadOp = render_attachments.depth->load_op,
                .storeOp = render_attachments.depth->store_op,
                .initialLayout = render_attachments.depth->load_op == VK_ATTACHMENT_LOAD_OP_CLEAR ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                .finalLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            });
    }

    VkSubpassDescription subpass;
    subpass.flags                = 0;
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments    = nullptr;
    subpass.colorAttachmentCount = static_cast<u32>(color_refs.size());
    subpass.pColorAttachments    = color_refs.data();
    subpass.pResolveAttachments  = nullptr;
    subpass.pDepthStencilAttachment = render_attachments.depth ? &depth_ref : nullptr;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments    = nullptr;

    VkRenderPassCreateInfo rp_info = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rp_info.attachmentCount        = static_cast<u32>(attachment_descriptions.size());
    rp_info.pAttachments           = attachment_descriptions.data();
    rp_info.subpassCount           = 1;
    rp_info.pSubpasses             = &subpass;
    rp_info.dependencyCount        = 0;
    rp_info.pDependencies          = nullptr;

    VkRenderPass vk_renderpass = VK_NULL_HANDLE;
    VK_CHECK(vkCreateRenderPass(device, &rp_info, nullptr, &vk_renderpass));

    return renderpasses.add({
            .vkhandle = vk_renderpass,
            .attachments = render_attachments,
        });
}

Handle<RenderPass> Device::find_or_create_renderpass(const RenderAttachments &render_attachments)
{
    for (auto &[handle, renderpass] : renderpasses)
    {
        if (renderpass->attachments == render_attachments)
        {
            return handle;
        }
    }

    return this->create_renderpass(render_attachments);
}

Handle<RenderPass> Device::find_or_create_renderpass(Handle<Framebuffer> framebuffer_handle)
{
    auto &framebuffer = *this->framebuffers.get(framebuffer_handle);

    RenderAttachments render_attachments;

    for (u32 i_image = 0; i_image < framebuffer.desc.attachments_format.size(); i_image++)
    {
        render_attachments.colors.push_back({.format = framebuffer.desc.attachments_format[i_image]});
    }

    if (framebuffer.desc.depth_format)
    {
        render_attachments.depth = {.format = framebuffer.desc.depth_format.value()};
    }

    for (auto &[handle, renderpass] : renderpasses)
    {
        if (renderpass->attachments == render_attachments)
        {
            return handle;
        }
    }

    return this->create_renderpass(render_attachments);
}

void Device::destroy_renderpass(Handle<RenderPass> renderpass_handle)
{
    if (auto *renderpass = renderpasses.get(renderpass_handle))
    {
        vkDestroyRenderPass(device, renderpass->vkhandle, nullptr);
        renderpasses.remove(renderpass_handle);
    }
}

/// --- Framebuffer

Handle<Framebuffer> Device::create_framebuffer(const FramebufferDescription &fb_desc)
{
    // Imageless framebuffer
    RenderAttachments render_attachments;
    Vec<VkFramebufferAttachmentImageInfo> image_infos;
    image_infos.reserve(fb_desc.attachments_format.size() + 1);

    for (u32 i_image = 0; i_image < fb_desc.attachments_format.size(); i_image++)
    {
        image_infos.emplace_back();
        auto &image_info           = image_infos.back();
        image_info                 = {.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO};
        image_info.flags           = 0;
        image_info.usage           = color_attachment_usage;
        image_info.width           = fb_desc.width;
        image_info.height          = fb_desc.height;
        image_info.layerCount      = fb_desc.layer_count;
        image_info.viewFormatCount = 1;
        image_info.pViewFormats    = &fb_desc.attachments_format[i_image];

        render_attachments.colors.push_back({.format = fb_desc.attachments_format[i_image]});
    }

    if (fb_desc.depth_format)
    {
        image_infos.emplace_back();
        auto &image_info           = image_infos.back();
        image_info                 = {.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO};
        image_info.flags           = 0;
        image_info.usage           = depth_attachment_usage;
        image_info.width           = fb_desc.width;
        image_info.height          = fb_desc.height;
        image_info.layerCount      = fb_desc.layer_count;
        image_info.viewFormatCount = 1;
        image_info.pViewFormats    = &fb_desc.depth_format.value();

        render_attachments.depth = {.format = fb_desc.depth_format.value()};
    }

    VkFramebufferAttachmentsCreateInfo attachments_info = {.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO};
    attachments_info.attachmentImageInfoCount           = static_cast<u32>(image_infos.size());
    attachments_info.pAttachmentImageInfos              = image_infos.data();

    auto &renderpass = *renderpasses.get(find_or_create_renderpass(render_attachments));

    VkFramebufferCreateInfo fb_info = {.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fb_info.pNext                   = &attachments_info;
    fb_info.flags                   = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT;
    fb_info.renderPass              = renderpass.vkhandle;
    fb_info.attachmentCount         = static_cast<u32>(image_infos.size());
    fb_info.width                   = fb_desc.width;
    fb_info.height                  = fb_desc.height;
    fb_info.layers                  = fb_desc.layer_count;

    VkFramebuffer vkhandle = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFramebuffer(device, &fb_info, nullptr, &vkhandle));

    return framebuffers.add({
            .vkhandle = vkhandle,
            .desc = fb_desc
        });
}

Handle<Framebuffer> Device::find_or_create_framebuffer(const FramebufferDescription &fb_desc)
{
    for (auto &[handle, framebuffer] : framebuffers)
    {
        if (framebuffer->desc == fb_desc)
        {
            return handle;
        }
    }
    return create_framebuffer(fb_desc);
}

void Device::destroy_framebuffer(Handle<Framebuffer> framebuffer_handle)
{
    if (auto *framebuffer = framebuffers.get(framebuffer_handle))
    {
        vkDestroyFramebuffer(device, framebuffer->vkhandle, nullptr);
        framebuffers.remove(framebuffer_handle);
    }
}

/// --- Graphics program

Handle<GraphicsProgram> Device::create_program(std::string name, const GraphicsState &graphics_state)
{
    DescriptorSet set = create_descriptor_set(*this, graphics_state.descriptors);

    std::array sets = {global_sets.uniform.layout, global_sets.sampled_images.layout, global_sets.storage_images.layout, set.layout};

    VkPushConstantRange push_constant_range;
    push_constant_range.stageFlags = VK_SHADER_STAGE_ALL;
    push_constant_range.offset     = 0;
    push_constant_range.size       = static_cast<u32>(push_constant_layout.size);

    VkPipelineLayoutCreateInfo pipeline_layout_info = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipeline_layout_info.setLayoutCount             = static_cast<u32>(sets.size());
    pipeline_layout_info.pSetLayouts                = sets.data();
    pipeline_layout_info.pushConstantRangeCount     = push_constant_range.size ? 1 : 0;
    pipeline_layout_info.pPushConstantRanges        = &push_constant_range;

    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout));

    VkPipelineCacheCreateInfo cache_info = {.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    VkPipelineCache cache = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineCache(device, &cache_info, nullptr, &cache));

    return graphics_programs.add({
        .name = name,
        .graphics_state = graphics_state,
        .pipeline_layout = pipeline_layout,
        .cache = cache,
        .descriptor_set = std::move(set),
    });
}

void Device::destroy_program(Handle<GraphicsProgram> program_handle)
{
    if (auto *program = graphics_programs.get(program_handle))
    {
        for (auto pipeline : program->pipelines)
        {
            vkDestroyPipeline(device, pipeline, nullptr);
        }

        vkDestroyPipelineCache(device, program->cache, nullptr);
        vkDestroyPipelineLayout(device, program->pipeline_layout, nullptr);

        destroy_descriptor_set(*this, program->descriptor_set);

        graphics_programs.remove(program_handle);
    }
}

unsigned Device::compile(Handle<GraphicsProgram> &program_handle, const RenderState &render_state)
{
    auto *p_program = graphics_programs.get(program_handle);
    assert(p_program);
    auto &program = *p_program;

    const auto &renderpass = *this->renderpasses.get(program.graphics_state.renderpass);

    Vec<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dyn_i = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn_i.dynamicStateCount                = static_cast<u32>(dynamic_states.size());
    dyn_i.pDynamicStates                   = dynamic_states.data();

    VkPipelineVertexInputStateCreateInfo vert_i = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    VkPipelineInputAssemblyStateCreateInfo asm_i = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .flags                  = 0,
        .topology               = to_vk(render_state.input_assembly.topology),
        .primitiveRestartEnable = VK_FALSE,
    };

    VkPipelineRasterizationConservativeStateCreateInfoEXT conservative = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT,
        .conservativeRasterizationMode    = VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT,
        .extraPrimitiveOverestimationSize = 0.1f, // in pixels
    };

    VkPipelineRasterizationStateCreateInfo rast_i = {
        .sType            = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext            = render_state.rasterization.enable_conservative_rasterization ? &conservative : nullptr,
        .flags            = 0,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .cullMode = VkCullModeFlags(render_state.rasterization.culling ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE),
        .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable         = render_state.depth.bias != 0.0f,
        .depthBiasConstantFactor = render_state.depth.bias,
        .depthBiasClamp          = 0,
        .depthBiasSlopeFactor    = 0,
        .lineWidth               = 1.0f,
    };

    rast_i.cullMode = VK_CULL_MODE_NONE;

    // TODO: from render_pass
    Vec<VkPipelineColorBlendAttachmentState> att_states;
    att_states.reserve(renderpass.attachments.colors.size());

    for (const auto &color_attachment : renderpass.attachments.colors)
    {
        UNUSED(color_attachment);

        att_states.emplace_back();
        auto &state          = att_states.back();
        state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
            | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT
            | VK_COLOR_COMPONENT_A_BIT;

        state.blendEnable         = VK_FALSE;

        if (render_state.alpha_blending)
        {
            // for now alpha_blending means "premultiplied alpha" for color and "additive" for alpha
            state.blendEnable         = VK_TRUE;
            state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            state.colorBlendOp        = VK_BLEND_OP_ADD;
            state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            state.alphaBlendOp        = VK_BLEND_OP_ADD;
        }
    }

    VkPipelineColorBlendStateCreateInfo colorblend_i = {.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorblend_i.flags             = 0;
    colorblend_i.attachmentCount   = static_cast<u32>(att_states.size());
    colorblend_i.pAttachments      = att_states.data();
    colorblend_i.logicOpEnable     = VK_FALSE;
    colorblend_i.logicOp           = VK_LOGIC_OP_COPY;
    colorblend_i.blendConstants[0] = 0.0f;
    colorblend_i.blendConstants[1] = 0.0f;
    colorblend_i.blendConstants[2] = 0.0f;
    colorblend_i.blendConstants[3] = 0.0f;

    VkPipelineViewportStateCreateInfo vp_i = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp_i.flags                             = 0;
    vp_i.viewportCount                     = 1;
    vp_i.scissorCount                      = 1;
    vp_i.pScissors                         = nullptr;
    vp_i.pViewports                        = nullptr;

    VkPipelineDepthStencilStateCreateInfo ds_i = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds_i.flags                 = 0;
    ds_i.depthTestEnable       = render_state.depth.test ? VK_TRUE : VK_FALSE;
    ds_i.depthWriteEnable      = render_state.depth.enable_write ? VK_TRUE : VK_FALSE;
    ds_i.depthCompareOp        = render_state.depth.test ? *render_state.depth.test : VK_COMPARE_OP_NEVER;
    ds_i.depthBoundsTestEnable = VK_FALSE;
    ds_i.minDepthBounds        = 0.0f;
    ds_i.maxDepthBounds        = 0.0f;
    ds_i.stencilTestEnable     = VK_FALSE;
    ds_i.back.failOp           = VK_STENCIL_OP_KEEP;
    ds_i.back.passOp           = VK_STENCIL_OP_KEEP;
    ds_i.back.compareOp        = VK_COMPARE_OP_ALWAYS;
    ds_i.back.compareMask      = 0;
    ds_i.back.reference        = 0;
    ds_i.back.depthFailOp      = VK_STENCIL_OP_KEEP;
    ds_i.back.writeMask        = 0;
    ds_i.front                 = ds_i.back;

    VkPipelineMultisampleStateCreateInfo ms_i = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms_i.flags                                = 0;
    ms_i.pSampleMask                          = nullptr;
    ms_i.rasterizationSamples                 = renderpass.attachments.colors.empty() ? VK_SAMPLE_COUNT_1_BIT : renderpass.attachments.colors[0].samples;
    ms_i.sampleShadingEnable                  = VK_FALSE;
    ms_i.alphaToCoverageEnable                = VK_FALSE;
    ms_i.alphaToOneEnable                     = VK_FALSE;
    ms_i.minSampleShading                     = .2f;

    Vec<VkPipelineShaderStageCreateInfo> shader_stages;
    shader_stages.reserve(3);

    if (program.graphics_state.vertex_shader.is_valid())
    {
        const auto &shader = *shaders.get(program.graphics_state.vertex_shader);
        VkPipelineShaderStageCreateInfo create_info = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        create_info.stage  = VK_SHADER_STAGE_VERTEX_BIT;
        create_info.module = shader.vkhandle;
        create_info.pName  = "main";
        shader_stages.push_back(create_info);
    }

    if (program.graphics_state.fragment_shader.is_valid())
    {
        const auto &shader = *shaders.get(program.graphics_state.fragment_shader);
        VkPipelineShaderStageCreateInfo create_info = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        create_info.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        create_info.module = shader.vkhandle;
        create_info.pName  = "main";
        shader_stages.push_back(create_info);
    }

    VkGraphicsPipelineCreateInfo pipe_i = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipe_i.layout                       = program.pipeline_layout;
    pipe_i.basePipelineHandle           = nullptr;
    pipe_i.basePipelineIndex            = 0;
    pipe_i.pVertexInputState            = &vert_i;
    pipe_i.pInputAssemblyState          = &asm_i;
    pipe_i.pRasterizationState          = &rast_i;
    pipe_i.pColorBlendState             = &colorblend_i;
    pipe_i.pTessellationState           = nullptr;
    pipe_i.pMultisampleState            = &ms_i;
    pipe_i.pDynamicState                = &dyn_i;
    pipe_i.pViewportState               = &vp_i;
    pipe_i.pDepthStencilState           = &ds_i;
    pipe_i.pStages                      = shader_stages.data();
    pipe_i.stageCount                   = static_cast<u32>(shader_stages.size());
    pipe_i.renderPass                   = renderpass.vkhandle;
    pipe_i.subpass                      = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipe_i, nullptr, &pipeline));


    if (this->vkSetDebugUtilsObjectNameEXT)
    {
        VkDebugUtilsObjectNameInfoEXT ni = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        ni.objectHandle                  = reinterpret_cast<u64>(pipeline);
        ni.objectType                    = VK_OBJECT_TYPE_PIPELINE;
        ni.pObjectName                   = program.name.c_str();
        VK_CHECK(this->vkSetDebugUtilsObjectNameEXT(device, &ni));
    }

    program.pipelines.push_back(pipeline);
    program.render_states.push_back(render_state);

    return static_cast<u32>(program.pipelines.size());
}
}
