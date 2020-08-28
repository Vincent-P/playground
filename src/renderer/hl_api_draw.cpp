#include "renderer/hl_api.hpp"
#include "renderer/vlk_context.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace my_app::vulkan
{

// TODO: multiple render targets, multisampling
static RenderPassH find_or_create_render_pass(API &api, PassInfo &&info)
{
    for (usize i = 0; i < api.renderpasses.size(); i++) {
        const auto &render_pass = api.renderpasses[i];
        if (render_pass.info == info) {
             return i;
        }
    }

    RenderPass rp;
    rp.info = std::move(info);

    std::vector<VkAttachmentDescription> attachments;

    VkAttachmentReference color_ref{.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    bool separateDepthStencilLayouts = api.ctx.vulkan12_features.separateDepthStencilLayouts;
    VkAttachmentReference depth_ref{.attachment = 0, .layout = separateDepthStencilLayouts ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL};

    if (rp.info.color) {
        color_ref.attachment = static_cast<u32>(attachments.size());

        VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkFormat format;

        auto color_rt  = api.get_rendertarget(rp.info.color->rt);
        if (color_rt.is_swapchain)
        {
            initial_layout = rp.info.color->load_op == VK_ATTACHMENT_LOAD_OP_CLEAR ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            format = api.ctx.swapchain.format.format;
        }
        else
        {
            auto &color_img = api.get_image(color_rt.image_h);
            initial_layout = get_src_image_access(color_img.usage).layout;
            format = color_img.info.format;
        }

        VkAttachmentDescription attachment;
        attachment.format         = format;
        attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp         = rp.info.color->load_op;
        attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout  = attachment.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR ? VK_IMAGE_LAYOUT_UNDEFINED : color_ref.layout;
        attachment.finalLayout    = color_ref.layout;
        attachment.flags          = {};
        attachments.push_back(std::move(attachment));
    }

    if (rp.info.depth) {
        depth_ref.attachment = static_cast<u32>(attachments.size());

        const auto &depth_rt    = api.get_rendertarget(rp.info.depth->rt);
        const auto &depth_image = api.get_image(depth_rt.image_h);

        VkAttachmentDescription attachment;
        attachment.format         = depth_image.info.format;
        attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp         = rp.info.depth->load_op;
        attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout  = rp.info.depth->load_op == VK_ATTACHMENT_LOAD_OP_CLEAR ? VK_IMAGE_LAYOUT_UNDEFINED : depth_ref.layout;
        attachment.finalLayout    = depth_ref.layout;
        attachment.flags          = {};
        attachments.push_back(std::move(attachment));
    }

    std::array<VkSubpassDescription, 1> subpasses{};
    subpasses[0].flags                = 0;
    subpasses[0].pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[0].inputAttachmentCount = 0;
    subpasses[0].pInputAttachments    = nullptr;
    subpasses[0].colorAttachmentCount = rp.info.color ? 1 : 0;
    subpasses[0].pColorAttachments    = rp.info.color ? &color_ref : nullptr;
    subpasses[0].pResolveAttachments  = nullptr;

    subpasses[0].pDepthStencilAttachment = rp.info.depth ? &depth_ref : nullptr;

    subpasses[0].preserveAttachmentCount = 0;
    subpasses[0].pPreserveAttachments    = nullptr;

    std::array<VkSubpassDependency, 0> dependencies{};

#if 0
    uint idep = 0;

    // Make anything that happened before starting the renderpass visible to
    // color and depth read/write operations to avoid RAW and WAW hazards
    dependencies[idep].srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependencies[idep].dstSubpass    = 0;
    dependencies[idep].srcStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dependencies[idep].srcAccessMask = 0;
    dependencies[idep].dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[idep].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
                     VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                     VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                     VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    idep++;

    // Make color and depth read/write operations visible to
    // compute/fragment shader read / renderpass load operations
    dependencies[idep].srcSubpass   = 0;
    dependencies[idep].dstSubpass   = VK_SUBPASS_EXTERNAL;
    dependencies[idep].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                                   | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                                   | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[idep].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[idep].dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    dependencies[idep].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
#endif

    VkRenderPassCreateInfo rp_info{};
    rp_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.attachmentCount = static_cast<u32>(attachments.size());
    rp_info.pAttachments    = attachments.data();
    rp_info.subpassCount    = subpasses.size();
    rp_info.pSubpasses      = subpasses.data();
    rp_info.dependencyCount = dependencies.size();
    rp_info.pDependencies   = dependencies.data();

    VK_CHECK(vkCreateRenderPass(api.ctx.device, &rp_info, nullptr, &rp.vkhandle));

    api.renderpasses.push_back(std::move(rp));
    return api.renderpasses.size() - 1;
}

static FrameBuffer &find_or_create_frame_buffer(API &api, const FrameBufferInfo &info, const RenderPass &render_pass)
{
    for (auto &framebuffer : api.framebuffers) {
        if (framebuffer.info == info) {
            return framebuffer;
        }
    }

    FrameBuffer fb;
    fb.info = info;

    std::vector<VkImageView> attachments;
    if (render_pass.info.color) {
        attachments.push_back(fb.info.image_view);
    }

    if (render_pass.info.depth) {
        attachments.push_back(fb.info.depth_view);
    }

    VkFramebufferCreateInfo ci{};
    ci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    ci.renderPass      = render_pass.vkhandle;
    ci.attachmentCount = static_cast<u32>(attachments.size());
    ci.pAttachments    = attachments.data();
    ci.layers          = 1;
    ci.width           = fb.info.width;
    ci.height          = fb.info.height;

    VK_CHECK(vkCreateFramebuffer(api.ctx.device, &ci, nullptr, &fb.vkhandle));

    api.framebuffers.push_back(std::move(fb));
    return api.framebuffers.back();
}

void API::begin_pass(PassInfo &&info)
{
    if (info.color) {
        assert(info.color->rt.is_valid());
    }

    auto render_pass_h = find_or_create_render_pass(*this, std::move(info));
    auto &render_pass = renderpasses[render_pass_h];

    FrameBufferInfo fb_info;

    if (render_pass.info.color) {
        bool is_swapchain = get_rendertarget(render_pass.info.color->rt).is_swapchain;
        if (!is_swapchain) {
            const auto &rt     = get_rendertarget(render_pass.info.color->rt);
            auto &image  = get_image(rt.image_h);
            fb_info.image_view = image.default_view;

            image.usage = ImageUsage::ColorAttachment; // TODO move?
        }
        else {
            fb_info.image_view = ctx.swapchain.get_current_image_view();
        }
    }

    if (render_pass.info.depth) {
        const auto &depth_rt    = get_rendertarget(render_pass.info.depth->rt);
        auto &depth_image = get_image(depth_rt.image_h);
        fb_info.depth_view      = depth_image.default_view;

        depth_image.usage = ImageUsage::DepthAttachment; // TODO move?
    }

    fb_info.render_pass = render_pass.vkhandle;

    if (render_pass.info.color) {
        const auto &rt = get_rendertarget(render_pass.info.color->rt);
        if (rt.is_swapchain) {
            fb_info.width  = ctx.swapchain.extent.width;
            fb_info.height = ctx.swapchain.extent.height;
        }
        else {
            const auto &image = get_image(rt.image_h);
            fb_info.width     = image.info.width;
            fb_info.height    = image.info.height;
        }
    }
    else if (render_pass.info.depth) {
        const auto &rt    = get_rendertarget(render_pass.info.depth->rt);
        const auto &image = get_image(rt.image_h);
        fb_info.width     = image.info.width;
        fb_info.height    = image.info.height;
    }
    else {
        fb_info.width  = 4096;
        fb_info.height = 4096;
    }

    auto &frame_buffer = find_or_create_frame_buffer(*this, fb_info, render_pass);

    auto &frame_resource = ctx.frame_resources.get_current();

    VkRect2D render_area{VkOffset2D(), {frame_buffer.info.width, frame_buffer.info.height}};

    std::vector<VkClearValue> clear_values;

    if (render_pass.info.color && render_pass.info.color->load_op == VK_ATTACHMENT_LOAD_OP_CLEAR) {
        VkClearValue clear;
        clear.color = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f} };
        clear_values.push_back(std::move(clear));
    }

    if (render_pass.info.depth && render_pass.info.depth->load_op == VK_ATTACHMENT_LOAD_OP_CLEAR) {
        // ??
        if (render_pass.info.color && clear_values.empty()) {
            VkClearValue clear;
            clear.color = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f} };
            clear_values.push_back(std::move(clear));
        }

        VkClearValue clear;
        clear.depthStencil = VkClearDepthStencilValue{.depth = 0.0f, .stencil = 0};
        clear_values.push_back(std::move(clear));
    }

    VkRenderPassBeginInfo rpbi{};
    rpbi.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpbi.renderArea      = render_area;
    rpbi.renderPass      = render_pass.vkhandle;
    rpbi.framebuffer     = frame_buffer.vkhandle;
    rpbi.clearValueCount = static_cast<u32>(clear_values.size());
    rpbi.pClearValues    = clear_values.data();

    current_render_pass = render_pass_h;

    vkCmdBeginRenderPass(frame_resource.command_buffer, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
}

void API::end_pass()
{
    auto &frame_resource = ctx.frame_resources.get_current();
    vkCmdEndRenderPass(frame_resource.command_buffer);

    current_render_pass = u32_invalid;
}

static VkPipeline find_or_create_pipeline(API &api, GraphicsProgram &program, PipelineInfo &pipeline_info)
{
    // const auto &program_info = pipeline_info.program_info;
    u32 pipeline_i           = u32_invalid;

    for (u32 i = 0; i < program.pipelines_info.size(); i++) {
        const auto &cur_pipeline_info = program.pipelines_info[i];
        if (cur_pipeline_info == pipeline_info) {
            pipeline_i = i;
            break;
        }
    }

    if (pipeline_i == u32_invalid) {
        std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        VkPipelineDynamicStateCreateInfo dyn_i{};
        dyn_i.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dyn_i.dynamicStateCount = dynamic_states.size();
        dyn_i.pDynamicStates    = dynamic_states.data();

        const auto &program_info       = pipeline_info.program_info;
        const auto &vertex_buffer_info = program_info.vertex_buffer_info;
        const auto &render_pass        = api.renderpasses[pipeline_info.render_pass];

        // TODO: support 0 or more than 1 vertex buffer
        // but full screen triangle already works without vertex buffer?
        std::array<VkVertexInputBindingDescription, 1> bindings;
        bindings[0].binding   = 0;
        bindings[0].stride    = vertex_buffer_info.stride;
        bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::vector<VkVertexInputAttributeDescription> attributes;
        attributes.reserve(vertex_buffer_info.vertices_info.size());

        u32 location = 0;
        for (const auto &vertex_info : vertex_buffer_info.vertices_info) {
            VkVertexInputAttributeDescription attribute;
            attribute.binding  = bindings[0].binding;
            attribute.location = location;
            attribute.format   = vertex_info.format;
            attribute.offset   = vertex_info.offset;
            attributes.push_back(std::move(attribute));
            location++;
        }

        VkPipelineVertexInputStateCreateInfo vert_i{};
        vert_i.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vert_i.vertexBindingDescriptionCount   = attributes.empty() ? 0 : bindings.size();
        vert_i.pVertexBindingDescriptions      = bindings.data();
        vert_i.vertexAttributeDescriptionCount = static_cast<u32>(attributes.size());
        vert_i.pVertexAttributeDescriptions    = attributes.data();

        VkPipelineInputAssemblyStateCreateInfo asm_i{};
        asm_i.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        asm_i.flags                  = {};
        asm_i.primitiveRestartEnable = VK_FALSE;
        asm_i.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;


        VkPipelineRasterizationConservativeStateCreateInfoEXT conservative{};
        conservative.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT;

        VkPipelineRasterizationStateCreateInfo rast_i{};
        rast_i.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

        if (program_info.enable_conservative_rasterization)
        {
            rast_i.pNext = &conservative;
            conservative.conservativeRasterizationMode = VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT;
            conservative.extraPrimitiveOverestimationSize = 0.1; // in pixels
        }

        rast_i.flags                   = {};
        rast_i.polygonMode             = VK_POLYGON_MODE_FILL;
        rast_i.cullMode                = VK_CULL_MODE_NONE;
        rast_i.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rast_i.depthClampEnable        = VK_FALSE;
        rast_i.rasterizerDiscardEnable = VK_FALSE;
        rast_i.depthBiasEnable         = program_info.depth_bias != 0.0f;
        rast_i.depthBiasConstantFactor = program_info.depth_bias;
        rast_i.depthBiasClamp          = 0;
        rast_i.depthBiasSlopeFactor    = 0;
        rast_i.lineWidth               = 1.0f;

        // TODO: from render_pass
        std::array<VkPipelineColorBlendAttachmentState, 1> att_states;
        att_states[0].colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        att_states[0].blendEnable         = VK_TRUE;
        att_states[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        att_states[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        att_states[0].colorBlendOp        = VK_BLEND_OP_ADD;
        att_states[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        att_states[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        att_states[0].alphaBlendOp        = VK_BLEND_OP_ADD;

        if (render_pass.info.color)
        {
            auto &color_attachment = *render_pass.info.color;
            auto &color = api.get_rendertarget(color_attachment.rt);
            if (!color.is_swapchain)
            {
                auto &image = api.get_image(color.image_h);
                // TODO: disable for all uint
                if (image.info.format == VK_FORMAT_R8_UINT) {
                    att_states[0].blendEnable = VK_FALSE;
                }
            }
        }


        VkPipelineColorBlendStateCreateInfo colorblend_i{};
        colorblend_i.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorblend_i.flags             = 0;
        colorblend_i.attachmentCount   = att_states.size();
        colorblend_i.pAttachments      = att_states.data();
        colorblend_i.logicOpEnable     = VK_FALSE;
        colorblend_i.logicOp           = VK_LOGIC_OP_COPY;
        colorblend_i.blendConstants[0] = 0.0f;
        colorblend_i.blendConstants[1] = 0.0f;
        colorblend_i.blendConstants[2] = 0.0f;
        colorblend_i.blendConstants[3] = 0.0f;

        VkPipelineViewportStateCreateInfo vp_i{};
        vp_i.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vp_i.flags         = 0;
        vp_i.viewportCount = 1;
        vp_i.scissorCount  = 1;
        vp_i.pScissors     = nullptr;
        vp_i.pViewports    = nullptr;

        VkPipelineDepthStencilStateCreateInfo ds_i{};
        ds_i.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        ds_i.flags                 = 0;
        ds_i.depthTestEnable       = pipeline_info.program_info.depth_test ? VK_TRUE : VK_FALSE;
        ds_i.depthWriteEnable      = pipeline_info.program_info.enable_depth_write ? VK_TRUE : VK_FALSE;
        ds_i.depthCompareOp        = pipeline_info.program_info.depth_test ? *pipeline_info.program_info.depth_test : VK_COMPARE_OP_NEVER;
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

        VkPipelineMultisampleStateCreateInfo ms_i{};
        ms_i.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms_i.flags                 = 0;
        ms_i.pSampleMask           = nullptr;
        ms_i.rasterizationSamples  = render_pass.info.samples;
        ms_i.sampleShadingEnable   = VK_FALSE;
        ms_i.alphaToCoverageEnable = VK_FALSE;
        ms_i.alphaToOneEnable      = VK_FALSE;
        ms_i.minSampleShading      = .2f;

        std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
        shader_stages.reserve(3);

        if (program_info.vertex_shader.is_valid())
        {
            const auto &shader = api.get_shader(program_info.vertex_shader);
            VkPipelineShaderStageCreateInfo create_info{};
            create_info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            create_info.stage  = VK_SHADER_STAGE_VERTEX_BIT;
            create_info.module = shader.vkhandle;
            create_info.pName  = "main";
            shader_stages.push_back(std::move(create_info));
        }

        if (program_info.geom_shader.is_valid())
        {
            const auto &shader = api.get_shader(program_info.geom_shader);
            VkPipelineShaderStageCreateInfo create_info{};
            create_info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            create_info.stage  = VK_SHADER_STAGE_GEOMETRY_BIT;
            create_info.module = shader.vkhandle;
            create_info.pName  = "main";
            shader_stages.push_back(std::move(create_info));
        }

        if (program_info.fragment_shader.is_valid())
        {
            const auto &shader = api.get_shader(program_info.fragment_shader);
            VkPipelineShaderStageCreateInfo create_info{};
            create_info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            create_info.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
            create_info.module = shader.vkhandle;
            create_info.pName  = "main";
            shader_stages.push_back(std::move(create_info));
        }

        VkGraphicsPipelineCreateInfo pipe_i{};
        pipe_i.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipe_i.layout              = pipeline_info.pipeline_layout;
        pipe_i.basePipelineHandle  = nullptr;
        pipe_i.basePipelineIndex   = 0;
        pipe_i.pVertexInputState   = &vert_i;
        pipe_i.pInputAssemblyState = &asm_i;
        pipe_i.pRasterizationState = &rast_i;
        pipe_i.pColorBlendState    = &colorblend_i;
        pipe_i.pTessellationState  = nullptr;
        pipe_i.pMultisampleState   = &ms_i;
        pipe_i.pDynamicState       = &dyn_i;
        pipe_i.pViewportState      = &vp_i;
        pipe_i.pDepthStencilState  = &ds_i;
        pipe_i.pStages             = shader_stages.data();
        pipe_i.stageCount          = static_cast<u32>(shader_stages.size());
        pipe_i.renderPass          = render_pass.vkhandle;
        pipe_i.subpass             = 0;

        program.pipelines_info.push_back(pipeline_info);

        program.pipelines_vk.emplace_back();
        auto &pipeline = program.pipelines_vk.back();
        VK_CHECK(vkCreateGraphicsPipelines(api.ctx.device, VK_NULL_HANDLE, 1, &pipe_i, nullptr, &pipeline));
        api.graphics_pipeline_count++;
        pipeline_i = static_cast<u32>(program.pipelines_vk.size()) - 1;

        std::cout << "new pipeline #" << pipeline_i << std::endl;
    }

    return program.pipelines_vk[pipeline_i];
}

static DescriptorSet &find_or_create_descriptor_set(API &api, GraphicsProgram &program, uint freq)
{
    for (usize i = 0; i < program.descriptor_sets[freq].size(); i++) {
        auto &descriptor_set = program.descriptor_sets[freq][i];
        if (descriptor_set.frame_used + api.ctx.frame_resources.data.size() < api.ctx.frame_count) {
            program.current_descriptor_set[freq] = i;
            return descriptor_set;
        }
    }

    DescriptorSet descriptor_set;

    VkDescriptorSetAllocateInfo dsai{};
    dsai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.descriptorPool     = api.ctx.descriptor_pool;
    dsai.pSetLayouts        = &program.descriptor_layouts[freq];
    dsai.descriptorSetCount = 1;
    api.ctx.descriptor_sets_count++;

    VK_CHECK(vkAllocateDescriptorSets(api.ctx.device, &dsai, &descriptor_set.set));
    descriptor_set.frame_used = api.ctx.frame_count;

    program.descriptor_sets[freq].push_back(std::move(descriptor_set));

    program.current_descriptor_set[freq] = program.descriptor_sets[freq].size() - 1;

    return program.descriptor_sets[freq].back();
}

static void undirty_descriptor_set(API &api, GraphicsProgram &program, uint i_set)
{
    if (program.data_dirty_by_set[i_set]) {
        auto &descriptor_set = find_or_create_descriptor_set(api, program, i_set);

        std::vector<VkWriteDescriptorSet> writes;
        map_transform(program.binded_data_by_set[i_set], writes, [&](const auto &binded_data) {
            assert(binded_data.has_value());
            VkWriteDescriptorSet write{};
            write.pNext            = nullptr;
            write.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet           = descriptor_set.set;
            write.dstBinding       = binded_data->binding;
            write.descriptorCount  = binded_data->images_info.empty() ? 1 : binded_data->images_info.size();
            write.descriptorType   = binded_data->type;
            write.pImageInfo       = binded_data->images_info.data();
            write.pBufferInfo      = &binded_data->buffer_info;
            write.pTexelBufferView = &binded_data->buffer_view;
            return write;
        });

        vkUpdateDescriptorSets(api.ctx.device, writes.size(), writes.data(), 0, nullptr);

        program.data_dirty_by_set[i_set] = false;
    }
}

static void bind_descriptor_set(API &api, GraphicsProgram &program, uint i_set)
{
    auto &frame_resource = api.ctx.frame_resources.get_current();
    auto &cmd             = frame_resource.command_buffer;

    /// --- Find and bind descriptor set
    undirty_descriptor_set(api, program, i_set);
    auto &descriptor_set      = program.descriptor_sets[i_set][program.current_descriptor_set[i_set]];
    descriptor_set.frame_used = api.ctx.frame_count;

    std::vector<u32> offsets;
    offsets.resize(program.dynamic_count_by_set[i_set]);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, program.pipeline_layout, i_set, 1, &descriptor_set.set, offsets.size(), offsets.data());
}

void API::bind_program(GraphicsProgramH H)
{
    assert(current_render_pass != u32_invalid);
    auto &frame_resource = ctx.frame_resources.get_current();
    auto &program        = get_program(H);

    /// --- Find and bind graphics pipeline
    PipelineInfo pipeline_info{program.info, program.pipeline_layout, current_render_pass};
    auto pipeline = find_or_create_pipeline(*this, program, pipeline_info);
    vkCmdBindPipeline(frame_resource.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    bind_descriptor_set(*this, program, GLOBAL_DESCRIPTOR_SET);
    bind_descriptor_set(*this, program, SHADER_DESCRIPTOR_SET);

    current_program = &program;
}

static void bind_image_internal(API &api, const std::vector<ImageH> &images_h, const std::vector<VkImageView> &images_view, std::vector<std::optional<ShaderBinding>> &binded_data, std::vector<BindingInfo> &bindings, bool &data_dirty, uint slot)
{
    assert(images_h.size() == images_view.size());

    if (binded_data.size() <= slot) {
        usize missing = slot - binded_data.size() + 1;
        for (usize i = 0; i < missing; i++) {
            binded_data.emplace_back(std::nullopt);
        }
    }

    assert(slot < binded_data.size());

    ShaderBinding data;
    data.binding                = slot;
    data.type                   = bindings[slot].type;

    for (usize i = 0; i < images_h.size(); i++)
    {
        auto &image = api.get_image(images_h[i]);
        const auto &image_view = images_view[i];

        assert(image.usage == ImageUsage::GraphicsShaderRead || image.usage == ImageUsage::GraphicsShaderReadWrite
               || image.usage == ImageUsage::ComputeShaderRead || image.usage == ImageUsage::ComputeShaderReadWrite);

        data.images_info.push_back({});
        auto &image_info = data.images_info.back();

        image_info.imageView   = image_view;
        image_info.sampler     = image.default_sampler;
        image_info.imageLayout = get_src_image_access(image.usage).layout;
    }

    if (!binded_data[slot].has_value() || *binded_data[slot] != data) {
        binded_data[slot] = std::move(data);
        data_dirty        = true;
    }
}

void API::bind_image(GraphicsProgramH program_h, uint set, uint slot, ImageH image_h, std::optional<VkImageView> image_view)
{
    auto &program = get_program(program_h);
    auto view = image_view ? *image_view : get_image(image_h).default_view;
    bind_image_internal(*this, {image_h}, {view}, program.binded_data_by_set[set], program.info.bindings_by_set[set], program.data_dirty_by_set[set], slot);
}

void API::bind_image(ComputeProgramH program_h, uint slot, ImageH image_h, std::optional<VkImageView> image_view)
{
    auto &program = get_program(program_h);
    auto view = image_view ? *image_view : get_image(image_h).default_view;
    bind_image_internal(*this, {image_h}, {view}, program.binded_data, program.info.bindings, program.data_dirty, slot);
}

void API::bind_images(GraphicsProgramH program_h, uint set, uint slot, const std::vector<ImageH> &images_h, const std::vector<VkImageView> &images_view)
{
    auto &program = get_program(program_h);
    bind_image_internal(*this, images_h, images_view, program.binded_data_by_set[set], program.info.bindings_by_set[set], program.data_dirty_by_set[set], slot);
}

void API::bind_images(ComputeProgramH program_h, uint slot, const std::vector<ImageH> &images_h, const std::vector<VkImageView> &images_view)
{
    auto &program = get_program(program_h);
    bind_image_internal(*this, images_h, images_view, program.binded_data, program.info.bindings, program.data_dirty, slot);
}

static void bind_combined_image_sampler_internal(API&, const std::vector<ImageH> &images_h, const std::vector<VkImageView> &images_view, Sampler &sampler, std::vector<std::optional<ShaderBinding>> &binded_data, std::vector<BindingInfo> &bindings, bool &data_dirty, uint slot)
{
    assert(images_h.size() == images_view.size());

    if (binded_data.size() <= slot) {
        usize missing = slot - binded_data.size() + 1;
        for (usize i = 0; i < missing; i++) {
            binded_data.emplace_back(std::nullopt);
        }
    }

    ShaderBinding data;
    data.binding                = slot;
    data.type                   = bindings[slot].type;

    for (usize i = 0; i < images_h.size(); i++)
    {
        const auto &image_view = images_view[i];

        data.images_info.push_back({});
        auto &image_info = data.images_info.back();

        image_info.imageView   = image_view;
        image_info.sampler     = sampler.vkhandle;
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    if (!binded_data[slot].has_value() || *binded_data[slot] != data) {
        binded_data[slot] = std::move(data);
        data_dirty        = true;
    }
}

void API::bind_combined_image_sampler(GraphicsProgramH program_h, uint set, uint slot, ImageH image_h, SamplerH sampler_h, std::optional<VkImageView> image_view)
{
    auto &program = get_program(program_h);
    auto &sampler = get_sampler(sampler_h);
    auto view = image_view ? *image_view : get_image(image_h).default_view;
    bind_combined_image_sampler_internal(*this, {image_h}, {view}, sampler, program.binded_data_by_set[set], program.info.bindings_by_set[set], program.data_dirty_by_set[set], slot);
}


void API::bind_combined_image_sampler(ComputeProgramH program_h, uint slot, ImageH image_h, SamplerH sampler_h, std::optional<VkImageView> image_view)
{
    auto &program = get_program(program_h);
    auto &sampler = get_sampler(sampler_h);
    auto view = image_view ? *image_view : get_image(image_h).default_view;
    bind_combined_image_sampler_internal(*this, {image_h}, {view}, sampler, program.binded_data, program.info.bindings, program.data_dirty, slot);
}


void API::bind_combined_images_sampler(GraphicsProgramH program_h, uint set, uint slot, const std::vector<ImageH> &images_h, SamplerH sampler_h, const std::vector<VkImageView> &images_view)
{
    auto &program = get_program(program_h);
    auto &sampler = get_sampler(sampler_h);
    bind_combined_image_sampler_internal(*this, images_h, images_view, sampler, program.binded_data_by_set[set], program.info.bindings_by_set[set], program.data_dirty_by_set[set], slot);
}

void API::bind_combined_images_sampler(ComputeProgramH program_h, uint slot, const std::vector<ImageH> &images_h, SamplerH sampler_h, const std::vector<VkImageView> &images_view)
{
    auto &program = get_program(program_h);
    auto &sampler = get_sampler(sampler_h);
    bind_combined_image_sampler_internal(*this, images_h, images_view, sampler, program.binded_data, program.info.bindings, program.data_dirty, slot);
}

static void bind_buffer_internal(API& /*api*/, Buffer &buffer, CircularBufferPosition &buffer_pos, std::vector<std::optional<ShaderBinding>> &binded_data, std::vector<BindingInfo> &bindings, bool &data_dirty, uint slot)
{
    if (binded_data.size() <= slot) {
        usize missing = slot - binded_data.size() + 1;
        for (usize i = 0; i < missing; i++) {
            binded_data.emplace_back(std::nullopt);
        }
    }

    assert(slot < binded_data.size());

    ShaderBinding data;
    data.binding            = slot;
    data.type               = bindings[slot].type;
    data.buffer_info.buffer = buffer.vkhandle;
    data.buffer_info.offset = buffer_pos.offset;
    data.buffer_info.range  = buffer_pos.length;

    binded_data[slot] = std::move(data);
    data_dirty        = true;
}

void API::bind_buffer(GraphicsProgramH program_h, uint set, uint slot, CircularBufferPosition buffer_pos)
{
    auto &program = get_program(program_h);
    auto &buffer  = get_buffer(buffer_pos.buffer_h);

    bind_buffer_internal(*this, buffer, buffer_pos, program.binded_data_by_set[set], program.info.bindings_by_set[set], program.data_dirty_by_set[set], slot);
}

void API::bind_buffer(ComputeProgramH program_h, uint slot, CircularBufferPosition buffer_pos)
{
    auto &program = get_program(program_h);
    auto &buffer  = get_buffer(buffer_pos.buffer_h);

    bind_buffer_internal(*this, buffer, buffer_pos, program.binded_data, program.info.bindings, program.data_dirty, slot);
}

void API::bind_vertex_buffer(BufferH H, u32 offset)
{
    const auto &vertex_buffer = get_buffer(H);
    auto &frame_resource      = ctx.frame_resources.get_current();
    VkDeviceSize device_offset = offset;
    vkCmdBindVertexBuffers(frame_resource.command_buffer, 0, 1, &vertex_buffer.vkhandle, &device_offset);
}

void API::bind_vertex_buffer(CircularBufferPosition v_pos)
{
    const auto &vertex_buffer = get_buffer(v_pos.buffer_h);
    auto &frame_resource      = ctx.frame_resources.get_current();
    vkCmdBindVertexBuffers(frame_resource.command_buffer, 0, 1, &vertex_buffer.vkhandle, &v_pos.offset);
}

void API::bind_index_buffer(BufferH H, u32 offset)
{
    const auto &index_buffer = get_buffer(H);
    auto &frame_resource     = ctx.frame_resources.get_current();
    vkCmdBindIndexBuffer(frame_resource.command_buffer, index_buffer.vkhandle, offset, VK_INDEX_TYPE_UINT16);
}

void API::bind_index_buffer(CircularBufferPosition i_pos)
{
    const auto &index_buffer = get_buffer(i_pos.buffer_h);
    auto &frame_resource     = ctx.frame_resources.get_current();
    vkCmdBindIndexBuffer(frame_resource.command_buffer, index_buffer.vkhandle, i_pos.offset, VK_INDEX_TYPE_UINT16);
}

void API::push_constant(VkShaderStageFlagBits stage, u32 offset, u32 size, void *data)
{
    assert(current_program);
    auto &frame_resource = ctx.frame_resources.get_current();
    const auto &program  = *current_program;
    vkCmdPushConstants(frame_resource.command_buffer, program.pipeline_layout, stage, offset, size, data);
}

static void pre_draw(API &api, GraphicsProgram &program)
{
    bind_descriptor_set(api, program, DRAW_DESCRIPTOR_SET);
}

void API::draw_indexed(u32 index_count, u32 instance_count, u32 first_index, i32 vertex_offset, u32 first_instance)
{
    pre_draw(*this, *current_program);
    auto &frame_resource = ctx.frame_resources.get_current();
    vkCmdDrawIndexed(frame_resource.command_buffer, index_count, instance_count, first_index, vertex_offset, first_instance);
}

void API::draw(u32 vertex_count, u32 instance_count, u32 first_vertex, u32 first_instance)
{
    pre_draw(*this, *current_program);
    auto &frame_resource = ctx.frame_resources.get_current();
    vkCmdDraw(frame_resource.command_buffer, vertex_count, instance_count, first_vertex, first_instance);
}

void API::set_scissor(const VkRect2D &scissor)
{
    auto &frame_resource = ctx.frame_resources.get_current();
    vkCmdSetScissor(frame_resource.command_buffer, 0, 1, &scissor);
}

void API::set_viewport(const VkViewport &viewport)
{
    auto &frame_resource = ctx.frame_resources.get_current();
    vkCmdSetViewport(frame_resource.command_buffer, 0, 1, &viewport);
}

void API::set_viewport_and_scissor(u32 width, u32 height)
{
    auto &frame_resource = ctx.frame_resources.get_current();

    VkViewport viewport{};
    viewport.width    = width;
    viewport.height   = height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(frame_resource.command_buffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent.width  = width;
    scissor.extent.height = height;
    vkCmdSetScissor(frame_resource.command_buffer, 0, 1, &scissor);
}

void API::begin_label(std::string_view name, float4 color)
{
    assert(name.size() > 0);
    assert(current_label.size() == 0);

    auto &frame_resource = ctx.frame_resources.get_current();
    VkDebugUtilsLabelEXT info{};
    info.sType     =VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    info.pLabelName = name.data();
    info.color[0] = color[0];
    info.color[1] = color[1];
    info.color[2] = color[2];
    info.color[3] = color[3];
    ctx.vkCmdBeginDebugUtilsLabelEXT(frame_resource.command_buffer, &info);

    current_label = name;
}

void API::add_timestamp(std::string_view label)
{
    auto &frame_resource = ctx.frame_resources.get_current();
    u32 frame_idx = ctx.frame_count % FRAMES_IN_FLIGHT;
    auto &current_timestamp_labels = timestamp_labels_per_frame[frame_idx];
    u32 offset    = frame_idx * MAX_TIMESTAMP_PER_FRAME + current_timestamp_labels.size();

    // write gpu timestamp
    vkCmdWriteTimestamp(frame_resource.command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, ctx.timestamp_pool, offset);

    // write cpu timestamp
    auto &cpu_timestamps = cpu_timestamps_per_frame[frame_idx];
    cpu_timestamps.push_back(Clock::now());

    current_timestamp_labels.push_back(label);
}

void API::end_label()
{
    add_timestamp(current_label);

    auto &frame_resource = ctx.frame_resources.get_current();
    ctx.vkCmdEndDebugUtilsLabelEXT(frame_resource.command_buffer);

    current_label = {};
}

// Should be used for debugging only as it creates a pipeline bubble
void API::global_barrier()
{
    auto cmd = ctx.frame_resources.get_current().command_buffer;

    VkMemoryBarrier mb;
    mb.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    mb.pNext         = nullptr;
    mb.srcAccessMask = 0;
    mb.dstAccessMask = 0;

    // TOP/BOTTOM OF PIPE define execution dependency and dont perform any memory accesses
    // srcAccess and dstAccess have to be 0
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         0,
                         1,
                         &mb,
                         0,
                         nullptr,
                         0,
                         nullptr);
}

static DescriptorSet &find_or_create_descriptor_set(API &api, ComputeProgram &program)
{
    for (usize i = 0; i < program.descriptor_sets.size(); i++) {
        auto &descriptor_set = program.descriptor_sets[i];
        if (descriptor_set.frame_used + api.ctx.frame_resources.data.size() < api.ctx.frame_count) {
            program.current_descriptor_set = i;
            return descriptor_set;
        }
    }

    DescriptorSet descriptor_set;

    VkDescriptorSetAllocateInfo dsai{};
    dsai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.descriptorPool     = api.ctx.descriptor_pool;
    dsai.pSetLayouts        = &program.descriptor_layout;
    dsai.descriptorSetCount = 1;
    api.ctx.descriptor_sets_count++;

    VK_CHECK(vkAllocateDescriptorSets(api.ctx.device, &dsai, &descriptor_set.set));
    descriptor_set.frame_used = api.ctx.frame_count;

    program.descriptor_sets.push_back(std::move(descriptor_set));

    program.current_descriptor_set = program.descriptor_sets.size() - 1;

    return program.descriptor_sets.back();
}

void API::dispatch(ComputeProgramH program_h, u32 x, u32 y, u32 z)
{
    auto &program        = get_program(program_h);
    auto &frame_resource = ctx.frame_resources.get_current();
    auto &cmd             = frame_resource.command_buffer;

    const auto &compute_shader = get_shader(program.info.shader);

    VkComputePipelineCreateInfo pinfo{};
    pinfo.sType        = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pinfo.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pinfo.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    pinfo.stage.module = compute_shader.vkhandle;
    pinfo.stage.pName  = "main";
    pinfo.layout       = program.pipeline_layout;

    usize pipeline_i = ~0u;
    for (usize i = 0; i < program.pipelines_info.size(); i++)
    {
        if (program.pipelines_info[i] == pinfo) {
            pipeline_i = i;
        }
    }

    if (pipeline_i == ~0u)
    {
        pipeline_i = program.pipelines_vk.size();
        program.pipelines_vk.emplace_back();
        auto &pipeline = program.pipelines_vk.back();
        VK_CHECK(vkCreateComputePipelines(ctx.device, VK_NULL_HANDLE, 1, &pinfo, nullptr, &pipeline));
        compute_pipeline_count++;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, program.pipelines_vk[pipeline_i]);

    /// --- Find and bind descriptor set
    if (program.data_dirty) {
        auto &descriptor_set = find_or_create_descriptor_set(*this, program);

        std::vector<VkWriteDescriptorSet> writes;

        for (const auto &binded_data : program.binded_data)
        {
            assert(binded_data.has_value());

            writes.emplace_back();
            VkWriteDescriptorSet &write = writes.back();
            write.pNext                 = nullptr;
            write.sType                 = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet                = descriptor_set.set;
            write.dstBinding            = binded_data->binding;
            write.descriptorCount       = binded_data->images_info.empty() ? 1 : binded_data->images_info.size();
            write.descriptorType        = binded_data->type;
            write.pImageInfo            = binded_data->images_info.data();
            write.pBufferInfo           = &binded_data->buffer_info;
            write.pTexelBufferView      = &binded_data->buffer_view;
        }

        vkUpdateDescriptorSets(ctx.device, writes.size(), writes.data(), 0, nullptr);

        program.data_dirty = false;
    }

    auto &descriptor_set      = program.descriptor_sets[program.current_descriptor_set];
    descriptor_set.frame_used = ctx.frame_count;

    std::vector<u32> offsets;
    offsets.resize(program.dynamic_count);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, program.pipeline_layout, 0, 1, &descriptor_set.set, offsets.size(), offsets.data());
    vkCmdDispatch(cmd, x, y, z);
}
} // namespace my_app::vulkan
