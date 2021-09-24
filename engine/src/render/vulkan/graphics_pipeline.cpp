#include "render/vulkan/pipelines.h"

#include "render/vulkan/device.h"
#include "render/vulkan/utils.h"

namespace vulkan
{

/// --- Renderpass

RenderPass create_renderpass(Device &device, const FramebufferFormat &format, std::span<const LoadOp> load_ops)
{
    auto attachments_count = format.attachments_format.size() + (format.depth_format.has_value() ? 1 : 0);
    ASSERT(load_ops.size() == attachments_count);

    Vec<VkAttachmentReference> color_refs;
    color_refs.reserve(format.attachments_format.size());

    Vec<VkAttachmentDescription> attachment_descriptions;
    attachment_descriptions.reserve(attachments_count);

    for (u32 i_color = 0; i_color < format.attachments_format.size(); i_color += 1)
    {
        color_refs.push_back({
                .attachment = static_cast<u32>(attachment_descriptions.size()),
                .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            });

        attachment_descriptions.push_back(VkAttachmentDescription{
            .format        = format.attachments_format[i_color],
            .samples       = VK_SAMPLE_COUNT_1_BIT,
            .loadOp        = to_vk(load_ops[i_color]),
            .storeOp       = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = load_ops[i_color].type == LoadOp::Type::Clear ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .finalLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        });
    }

    VkAttachmentReference depth_ref{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    if (format.depth_format.has_value())
    {
        depth_ref.attachment = static_cast<u32>(attachment_descriptions.size());

        attachment_descriptions.push_back(VkAttachmentDescription{
                .format        = format.depth_format.value(),
                .samples       = VK_SAMPLE_COUNT_1_BIT,
                .loadOp        = to_vk(load_ops.back()),
                .storeOp       = VK_ATTACHMENT_STORE_OP_STORE,
                .initialLayout = load_ops.back().type == LoadOp::Type::Clear ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                .finalLayout   = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
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
    subpass.pDepthStencilAttachment = format.depth_format.has_value() ? &depth_ref : nullptr;
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
    VK_CHECK(vkCreateRenderPass(device.device, &rp_info, nullptr, &vk_renderpass));

    return {.vkhandle = vk_renderpass, .load_ops = std::vector(load_ops.begin(), load_ops.end())};
}

RenderPass &Device::find_or_create_renderpass(Framebuffer &framebuffer, std::span<const LoadOp> load_ops)
{
    ASSERT(framebuffer.color_attachments.size() == framebuffer.format.attachments_format.size());
    ASSERT(framebuffer.depth_attachment.is_valid() == framebuffer.format.depth_format.has_value());

    for (auto &renderpass : framebuffer.renderpasses)
    {
        if (std::ranges::equal(renderpass.load_ops, load_ops))
        {
            return renderpass;
        }
    }

    framebuffer.renderpasses.push_back(create_renderpass(*this, framebuffer.format, load_ops));
    return framebuffer.renderpasses.back();
}

/// --- Framebuffer

Handle<Framebuffer> Device::create_framebuffer(const FramebufferFormat &fb_desc, std::span<const Handle<Image>> color_attachments, Handle<Image> depth_attachment)
{
    Framebuffer fb = {};
    fb.format = fb_desc;
    fb.color_attachments = {color_attachments.begin(), color_attachments.end()};
    fb.depth_attachment = depth_attachment;

    ASSERT(fb.format.attachments_format.empty());
    ASSERT(fb.format.depth_format.has_value() == false);

    auto attachments_count = fb.color_attachments.size() + (fb.depth_attachment.is_valid() ? 1 : 0);

    Vec<VkImageView> attachment_views;
    attachment_views.reserve(attachments_count);
    for (auto &attachment : fb.color_attachments)
    {
        auto &image = *this->images.get(attachment);
        attachment_views.push_back(image.full_view.vkhandle);
        fb.format.attachments_format.push_back(image.desc.format);
    }
    if (fb.depth_attachment.is_valid())
    {
        auto &image = *this->images.get(fb.depth_attachment);
        attachment_views.push_back(image.full_view.vkhandle);
        fb.format.depth_format = image.desc.format;
    }

    auto &renderpass = this->find_or_create_renderpass(fb, Vec<LoadOp>(attachments_count, LoadOp::ignore()));

    VkFramebufferCreateInfo fb_info = {.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fb_info.renderPass              = renderpass.vkhandle;
    fb_info.attachmentCount         = static_cast<u32>(attachments_count);
    fb_info.pAttachments            = attachment_views.data();
    fb_info.width                   = static_cast<u32>(fb.format.width);
    fb_info.height                  = static_cast<u32>(fb.format.height);
    fb_info.layers                  = fb.format.layer_count;

    fb.vkhandle = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFramebuffer(device, &fb_info, nullptr, &fb.vkhandle));

    return framebuffers.add(std::move(fb));
}

void Device::destroy_framebuffer(Handle<Framebuffer> framebuffer_handle)
{
    if (auto *framebuffer = framebuffers.get(framebuffer_handle))
    {
        vkDestroyFramebuffer(device, framebuffer->vkhandle, nullptr);

        for (auto &renderpass : framebuffer->renderpasses)
        {
            vkDestroyRenderPass(device, renderpass.vkhandle, nullptr);
        }
        framebuffers.remove(framebuffer_handle);
    }
}

/// --- Graphics program

Handle<GraphicsProgram> Device::create_program(std::string name, const GraphicsState &graphics_state)
{
    DescriptorSet set = create_descriptor_set(*this, graphics_state.descriptors);

    std::array sets = {
        global_sets.uniform.layout,
        global_sets.sampled_images.layout,
        global_sets.storage_images.layout,
        global_sets.storage_buffers.layout,
        set.layout,
    };

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

    auto attachments_count = graphics_state.attachments_format.attachments_format.size() + (graphics_state.attachments_format.depth_format.has_value() ? 1 : 0);
    auto renderpass = create_renderpass(*this, graphics_state.attachments_format, Vec<LoadOp>(attachments_count, LoadOp::ignore()));

    return graphics_programs.add({
        .name = name,
        .graphics_state = graphics_state,
        .pipeline_layout = pipeline_layout,
        .cache = cache,
        .renderpass = renderpass.vkhandle,
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
        vkDestroyRenderPass(device, program->renderpass, nullptr);

        destroy_descriptor_set(*this, program->descriptor_set);

        graphics_programs.remove(program_handle);
    }
}

unsigned Device::compile(Handle<GraphicsProgram> &program_handle, const RenderState &render_state)
{
    auto *p_program = graphics_programs.get(program_handle);
    ASSERT(p_program);
    auto &program = *p_program;

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

    Vec<VkPipelineColorBlendAttachmentState> att_states;
    att_states.reserve(program.graphics_state.attachments_format.attachments_format.size());

    for (const auto &color_attachment : program.graphics_state.attachments_format.attachments_format)
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
    ms_i.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;
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
    pipe_i.renderPass                   = program.renderpass;
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
