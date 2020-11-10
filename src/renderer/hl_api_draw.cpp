#include "renderer/hl_api.hpp"
#include "renderer/vlk_context.hpp"
#include <cstddef>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace my_app::vulkan
{

/// --- Renderpass

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

    std::vector<VkAttachmentReference> color_refs;
    color_refs.reserve(rp.info.colors.size());

    for (auto &color_attachment : rp.info.colors)
    {
        color_refs.emplace_back();
        auto &color_ref = color_refs.back();

        color_ref.attachment = static_cast<u32>(attachments.size());
        color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        const auto &color_view = api.get_image_view(color_attachment.image_view);

        attachments.emplace_back();
        auto &attachment = attachments.back();
        attachment.flags          = 0;
        attachment.format         = color_view.format;
        attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp         = color_attachment.load_op;
        attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout  = attachment.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR ? VK_IMAGE_LAYOUT_UNDEFINED : color_ref.layout;
        attachment.finalLayout    = color_ref.layout;
    }

    bool separateDepthStencilLayouts = api.ctx.vulkan12_features.separateDepthStencilLayouts;
    VkAttachmentReference depth_ref{.attachment = 0, .layout = separateDepthStencilLayouts ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL};
    if (rp.info.depth) {
        depth_ref.attachment = static_cast<u32>(attachments.size());

        const auto &depth_view    = api.get_image_view(rp.info.depth->image_view);

        VkAttachmentDescription attachment;
        attachment.format         = depth_view.format;
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
    subpasses[0].colorAttachmentCount = color_refs.size();
    subpasses[0].pColorAttachments    = color_refs.data();
    subpasses[0].pResolveAttachments  = nullptr;

    subpasses[0].pDepthStencilAttachment = rp.info.depth ? &depth_ref : nullptr;

    subpasses[0].preserveAttachmentCount = 0;
    subpasses[0].pPreserveAttachments    = nullptr;

    VkRenderPassCreateInfo rp_info = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rp_info.attachmentCount        = static_cast<u32>(attachments.size());
    rp_info.pAttachments           = attachments.data();
    rp_info.subpassCount           = subpasses.size();
    rp_info.pSubpasses             = subpasses.data();
    rp_info.dependencyCount        = 0;
    rp_info.pDependencies          = nullptr;

    VK_CHECK(vkCreateRenderPass(api.ctx.device, &rp_info, nullptr, &rp.vkhandle));

    api.renderpasses.push_back(std::move(rp));
    return api.renderpasses.size() - 1;
}

static FrameBuffer &find_or_create_frame_buffer(API &api, const RenderPass &render_pass)
{
    VkFramebufferCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    ci.renderPass              = render_pass.vkhandle;

    std::vector<VkImageView> attachments;

    for (const auto &color_attachment : render_pass.info.colors)
    {
        const auto &color_view = api.get_image_view(color_attachment.image_view);
        attachments.push_back(color_view.vkhandle);
    }

    if (render_pass.info.depth)
    {
        const auto &depth_view = api.get_image_view(render_pass.info.depth->image_view);
        attachments.push_back(depth_view.vkhandle);
    }

    ci.attachmentCount = static_cast<u32>(attachments.size());
    ci.pAttachments    = attachments.data();

    for (const auto &color_attachment : render_pass.info.colors)
    {
        const auto &color_view = api.get_image_view(color_attachment.image_view);
        const auto &image      = api.get_image(color_view.image_h);
        ci.layers              = image.info.layers;
        ci.width               = image.info.width;
        ci.height              = image.info.height;
    }

    if (render_pass.info.depth)
    {
        const auto &depth_view    = api.get_image_view(render_pass.info.depth->image_view);
        const auto &image = api.get_image(depth_view.image_h);
        ci.layers              = image.info.layers;
        ci.width               = image.info.width;
        ci.height              = image.info.height;
    }
    else if (ci.width == 0 || ci.height == 0)
    {
        // shouldnt happen
        ci.layers              = 1;
        ci.width               = 4096;
        ci.height              = 4096;
    }

    for (auto &framebuffer : api.framebuffers) {
        if (framebuffer.create_info == ci) {
            return framebuffer;
        }
    }

    FrameBuffer fb;
    fb.create_info = ci;
    VK_CHECK(vkCreateFramebuffer(api.ctx.device, &ci, nullptr, &fb.vkhandle));
    api.framebuffers.push_back(std::move(fb));
    return api.framebuffers.back();
}

void API::begin_pass(PassInfo &&info)
{
    auto render_pass_h = find_or_create_render_pass(*this, std::move(info));
    auto &render_pass = renderpasses[render_pass_h];

    auto &frame_buffer = find_or_create_frame_buffer(*this, render_pass);

    auto &frame_resource = ctx.frame_resources.get_current();

    VkRect2D render_area{VkOffset2D(), {frame_buffer.create_info.width, frame_buffer.create_info.height}};

    std::vector<VkClearValue> clear_values;

    for (auto &color_attachment : render_pass.info.colors) {
        if (color_attachment.load_op == VK_ATTACHMENT_LOAD_OP_CLEAR) {
            clear_values.emplace_back();
            auto &clear = clear_values.back();
            clear.color = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f} };
        }
    }

    if (render_pass.info.depth && render_pass.info.depth->load_op == VK_ATTACHMENT_LOAD_OP_CLEAR) {
        // maybe push clear colors even when there is no need to clear attachments?
        if (!render_pass.info.colors.empty() && clear_values.empty()) {
            for (auto &color_attachment : render_pass.info.colors) {
                if (color_attachment.load_op == VK_ATTACHMENT_LOAD_OP_CLEAR) {
                    clear_values.emplace_back();
                    auto &clear = clear_values.back();
                    clear.color = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f} };
                }
            }
        }

        VkClearValue clear;
        clear.depthStencil = VkClearDepthStencilValue{.depth = 0.0f, .stencil = 0};
        clear_values.push_back(std::move(clear));
    }

    VkRenderPassBeginInfo rpbi = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpbi.renderArea            = render_area;
    rpbi.renderPass            = render_pass.vkhandle;
    rpbi.framebuffer           = frame_buffer.vkhandle;
    rpbi.clearValueCount       = static_cast<u32>(clear_values.size());
    rpbi.pClearValues          = clear_values.data();

    current_render_pass = render_pass_h;

    vkCmdBeginRenderPass(frame_resource.command_buffer, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
}

void API::end_pass()
{
    auto &frame_resource = ctx.frame_resources.get_current();
    vkCmdEndRenderPass(frame_resource.command_buffer);

    current_render_pass = u32_invalid;
}

/// --- Pipeline

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

        VkPipelineDynamicStateCreateInfo dyn_i = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dyn_i.dynamicStateCount                = dynamic_states.size();
        dyn_i.pDynamicStates                   = dynamic_states.data();

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

        VkPipelineVertexInputStateCreateInfo vert_i
            = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vert_i.vertexBindingDescriptionCount   = attributes.empty() ? 0 : bindings.size();
        vert_i.pVertexBindingDescriptions      = bindings.data();
        vert_i.vertexAttributeDescriptionCount = static_cast<u32>(attributes.size());
        vert_i.pVertexAttributeDescriptions    = attributes.data();

        VkPipelineInputAssemblyStateCreateInfo asm_i
            = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        asm_i.flags                  = 0;
        asm_i.primitiveRestartEnable = VK_FALSE;
        asm_i.topology               = vk_topology_from_enum(program.info.topology);

        VkPipelineRasterizationConservativeStateCreateInfoEXT conservative = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT};

        VkPipelineRasterizationStateCreateInfo rast_i
            = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};

        if (program_info.enable_conservative_rasterization)
        {
            rast_i.pNext = &conservative;
            conservative.conservativeRasterizationMode = VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT;
            conservative.extraPrimitiveOverestimationSize = 0.1; // in pixels
        }

        rast_i.flags                   = 0;
        rast_i.polygonMode             = VK_POLYGON_MODE_FILL;
        rast_i.cullMode                = VK_CULL_MODE_NONE;
        rast_i.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rast_i.depthClampEnable        = VK_TRUE;
        rast_i.rasterizerDiscardEnable = VK_FALSE;
        rast_i.depthBiasEnable         = program_info.depth_bias != 0.0f;
        rast_i.depthBiasConstantFactor = program_info.depth_bias;
        rast_i.depthBiasClamp          = 0;
        rast_i.depthBiasSlopeFactor    = 0;
        rast_i.lineWidth               = 1.0f;

        // TODO: from render_pass
        std::vector<VkPipelineColorBlendAttachmentState> att_states;
        att_states.reserve(render_pass.info.colors.size());

        for (const auto &color_attachment : render_pass.info.colors)
        {
            auto &color_view = api.get_image_view(color_attachment.image_view);

            att_states.emplace_back();
            auto &state = att_states.back();
            state.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            state.blendEnable         = color_view.format != VK_FORMAT_R8_UINT; // TODO: disable for all uint
            state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            state.colorBlendOp        = VK_BLEND_OP_ADD;
            state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            state.alphaBlendOp        = VK_BLEND_OP_ADD;
        }

        VkPipelineColorBlendStateCreateInfo colorblend_i
            = {.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        colorblend_i.flags             = 0;
        colorblend_i.attachmentCount   = att_states.size();
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

        VkPipelineDepthStencilStateCreateInfo ds_i
            = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        ds_i.flags            = 0;
        ds_i.depthTestEnable  = pipeline_info.program_info.depth_test ? VK_TRUE : VK_FALSE;
        ds_i.depthWriteEnable = pipeline_info.program_info.enable_depth_write ? VK_TRUE : VK_FALSE;
        ds_i.depthCompareOp
            = pipeline_info.program_info.depth_test ? *pipeline_info.program_info.depth_test : VK_COMPARE_OP_NEVER;
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
        ms_i.rasterizationSamples                 = render_pass.info.samples;
        ms_i.sampleShadingEnable                  = VK_FALSE;
        ms_i.alphaToCoverageEnable                = VK_FALSE;
        ms_i.alphaToOneEnable                     = VK_FALSE;
        ms_i.minSampleShading                     = .2f;

        std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
        shader_stages.reserve(3);

        if (program_info.vertex_shader.is_valid())
        {
            const auto &shader = api.get_shader(program_info.vertex_shader);
            VkPipelineShaderStageCreateInfo create_info
                = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            create_info.stage  = VK_SHADER_STAGE_VERTEX_BIT;
            create_info.module = shader.vkhandle;
            create_info.pName  = "main";
            shader_stages.push_back(std::move(create_info));
        }

        if (program_info.geom_shader.is_valid())
        {
            const auto &shader = api.get_shader(program_info.geom_shader);
            VkPipelineShaderStageCreateInfo create_info
                = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            create_info.stage  = VK_SHADER_STAGE_GEOMETRY_BIT;
            create_info.module = shader.vkhandle;
            create_info.pName  = "main";
            shader_stages.push_back(std::move(create_info));
        }

        if (program_info.fragment_shader.is_valid())
        {
            const auto &shader = api.get_shader(program_info.fragment_shader);
            VkPipelineShaderStageCreateInfo create_info
                = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            create_info.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
            create_info.module = shader.vkhandle;
            create_info.pName  = "main";
            shader_stages.push_back(std::move(create_info));
        }

        VkGraphicsPipelineCreateInfo pipe_i = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipe_i.layout                       = pipeline_info.pipeline_layout;
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
        pipe_i.renderPass                   = render_pass.vkhandle;
        pipe_i.subpass                      = 0;

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

/// --- Descriptor set

static DescriptorSet &find_or_create_descriptor_set(API &api, ShaderBindingSet& binding_set)
{
    for (usize i = 0; i < binding_set.descriptor_sets.size(); i++) {
        auto &descriptor_set = binding_set.descriptor_sets[i];
        if (descriptor_set.frame_used + api.ctx.frame_resources.data.size() < api.ctx.frame_count) {
            binding_set.current_descriptor_set = i;
            return descriptor_set;
        }
    }

    binding_set.descriptor_sets.emplace_back();
    DescriptorSet &descriptor_set = binding_set.descriptor_sets.back();

    VkDescriptorSetAllocateInfo dsai = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsai.descriptorPool              = api.ctx.descriptor_pool;
    dsai.pSetLayouts                 = &binding_set.descriptor_layout;
    dsai.descriptorSetCount          = 1;
    api.ctx.descriptor_sets_count++;

    VK_CHECK(vkAllocateDescriptorSets(api.ctx.device, &dsai, &descriptor_set.set));
    descriptor_set.frame_used = api.ctx.frame_count;

    binding_set.current_descriptor_set = binding_set.descriptor_sets.size() - 1;

    return binding_set.descriptor_sets.back();
}

static void undirty_descriptor_set(API &api, ShaderBindingSet &binding_set)
{
    if (binding_set.data_dirty) {
        auto &descriptor_set = find_or_create_descriptor_set(api, binding_set);

        std::vector<VkWriteDescriptorSet> writes;
        map_transform(binding_set.binded_data, writes, [&](const auto &binded_data) {
            assert(binded_data.has_value());
            VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            write.dstSet               = descriptor_set.set;
            write.dstBinding           = binded_data->binding;
            write.descriptorCount      = binded_data->images_info.empty() ? 1 : binded_data->images_info.size();
            write.descriptorType       = binded_data->type;
            write.pImageInfo           = binded_data->images_info.data();
            write.pBufferInfo          = &binded_data->buffer_info;
            write.pTexelBufferView     = &binded_data->buffer_view;
            return write;
        });

        vkUpdateDescriptorSets(api.ctx.device, writes.size(), writes.data(), 0, nullptr);

        binding_set.data_dirty = false;
    }
}

void API::bind_program(GraphicsProgramH H)
{
    assert(current_render_pass != u32_invalid);
    auto &frame_resource = ctx.frame_resources.get_current();
    VkCommandBuffer cmd  = frame_resource.command_buffer;
    auto &program        = get_program(H);

    /// --- Find and bind graphics pipeline
    PipelineInfo pipeline_info{program.info, program.pipeline_layout, current_render_pass};
    VkPipeline pipeline = find_or_create_pipeline(*this, program, pipeline_info);
    vkCmdBindPipeline(frame_resource.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // prepare global descriptor set
    auto &global_set      = get_descriptor_set(global_bindings.binding_set);
    global_set.frame_used = ctx.frame_count;

    // prepare shader descriptor set
    auto &binding_set = program.binding_sets_by_freq[SHADER_DESCRIPTOR_SET - 1];
    undirty_descriptor_set(*this, binding_set);
    auto &shader_set      = get_descriptor_set(binding_set);
    shader_set.frame_used = ctx.frame_count;

    std::array descriptor_sets = {global_set.set, shader_set.set};

    auto dynamic_offsets = global_bindings.binding_set.dynamic_offsets;
    dynamic_offsets.insert(dynamic_offsets.end(), binding_set.dynamic_offsets.begin(), binding_set.dynamic_offsets.end());

    // bind both sets
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            program.pipeline_layout,
                            0,
                            descriptor_sets.size(),
                            descriptor_sets.data(),
                            dynamic_offsets.size(),
                            dynamic_offsets.data());

    current_program = &program;
}

// bind one image
// sampler ? => combined image sampler descriptor
// else storage image
static void bind_image_internal(API &api, ShaderBindingSet &binding_set, uint slot, uint index, ImageViewH &image_view_h, std::optional<SamplerH> sampler = std::nullopt)
{
    assert(slot < binding_set.binded_data.size());

    if (!binding_set.binded_data[slot].has_value())
    {
        binding_set.binded_data[slot] = std::make_optional<BindingData>({});
        binding_set.binded_data[slot]->images_info.resize(1); // we are going to fill it
    }
    else
    {
        assert(index < binding_set.binded_data[slot]->images_info.size());
    }

    auto &data = binding_set.binded_data[slot];

    data->binding = slot;
    data->type    = binding_set.bindings_info[slot].type;

    assert(sampler ? data->type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : data->type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    auto &image_view = api.get_image_view(image_view_h);
    auto &image      = api.get_image(image_view.image_h);
    (void)(image);

    assert(sampler
           ? image.usage == ImageUsage::GraphicsShaderRead      || image.usage == ImageUsage::ComputeShaderRead
           : image.usage == ImageUsage::GraphicsShaderReadWrite || image.usage == ImageUsage::ComputeShaderReadWrite);

    auto &image_info       = data->images_info[index];
    image_info.imageView   = image_view.vkhandle;
    image_info.imageLayout = sampler ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
    image_info.sampler     = sampler ? api.get_sampler(*sampler).vkhandle : VK_NULL_HANDLE;

    binding_set.data_dirty = true;
}

// bind several images at once
// if samplers.size == 0 then bind several storage images
// if samplers.size == 1 then bind several combined image samplers with the same sampler
// else bind several combined image samplers with a diferent sampler for each image
static void bind_images_internal(API &api, ShaderBindingSet &binding_set, uint slot, const std::vector<ImageViewH> &image_views_h, const std::vector<SamplerH> &samplers_h = {})
{
    assert(slot < binding_set.binded_data.size());
    assert(samplers_h.empty() || samplers_h.size() == 1 || samplers_h.size() == image_views_h.size());

    BindingData data;
    data.binding = slot;
    data.type    = binding_set.bindings_info[slot].type;

    assert(!samplers_h.empty() ? data.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : data.type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    for (usize i = 0; i < image_views_h.size(); i++)
    {
        auto &image_view = api.get_image_view(image_views_h[i]);
        auto &image = api.get_image(image_view.image_h);
        (void)(image);

        assert(!samplers_h.empty()
           ? image.usage == ImageUsage::GraphicsShaderRead      || image.usage == ImageUsage::ComputeShaderRead
           : image.usage == ImageUsage::GraphicsShaderReadWrite || image.usage == ImageUsage::ComputeShaderReadWrite);

        data.images_info.push_back({});
        auto &image_info = data.images_info.back();

        image_info.imageView   = image_view.vkhandle;
        image_info.imageLayout = !samplers_h.empty() ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
        image_info.sampler     = VK_NULL_HANDLE;

        if (samplers_h.size() == 1)
        {
            image_info.sampler = api.get_sampler(samplers_h[0]).vkhandle;
        }
        else if (!samplers_h.empty())
        {
            image_info.sampler = api.get_sampler(samplers_h[i]).vkhandle;
        }
    }

    if (!binding_set.binded_data[slot].has_value() || *binding_set.binded_data[slot] != data) {
        binding_set.binded_data[slot] = std::move(data);
        binding_set.data_dirty        = true;
    }
}

// todo handle dynamic buffer correctly wtf
static void bind_buffer_internal(API & /*api*/, ShaderBindingSet &binding_set, Buffer &buffer, CircularBufferPosition &buffer_pos, uint slot)
{
    assert(slot < binding_set.binded_data.size());

    auto binding_type = binding_set.bindings_info[slot].type;
    bool is_dynamic = binding_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    assert(is_dynamic);

    if (!binding_set.binded_data[slot])
    {
        BindingData data;
        data.binding            = slot;
        data.type               = binding_type;
        data.buffer_info.buffer = buffer.vkhandle;
        data.buffer_info.offset = 0;
        data.buffer_info.range  = buffer_pos.length;

        binding_set.binded_data[slot] = std::move(data);
        binding_set.data_dirty        = true;
    }

    if (is_dynamic)
    {
        usize offset_idx = 0;
        for (usize i = 0; i < binding_set.dynamic_bindings.size(); i++) {
            if (binding_set.dynamic_bindings[i] == slot) {
                offset_idx = i;
                break;
            }
        }
        binding_set.dynamic_offsets[offset_idx] = buffer_pos.offset;
    }
}


// storage images
void API::bind_image(GraphicsProgramH program_h, ImageViewH image_view_h, uint set, uint slot, uint index)
{
    assert(!(program_h.is_valid() && set == GLOBAL_DESCRIPTOR_SET));
    auto &binding_set = set == GLOBAL_DESCRIPTOR_SET ? global_bindings.binding_set : get_program(program_h).binding_sets_by_freq[set-1];
    bind_image_internal(*this, binding_set, slot, index, image_view_h);
}

void API::bind_image(ComputeProgramH program_h, ImageViewH image_view_h, uint slot, uint index)
{
    auto &binding_set = get_program(program_h).binding_set;
    bind_image_internal(*this, binding_set, slot, index, image_view_h);
}

void API::bind_images(GraphicsProgramH program_h, const std::vector<ImageViewH> &image_views_h, uint set, uint slot)
{
    assert(!(program_h.is_valid() && set == GLOBAL_DESCRIPTOR_SET));
    auto &binding_set = set == GLOBAL_DESCRIPTOR_SET ? global_bindings.binding_set : get_program(program_h).binding_sets_by_freq[set-1];
    bind_images_internal(*this, binding_set, slot, image_views_h);
}

void API::bind_images(ComputeProgramH program_h, const std::vector<ImageViewH> &image_views_h, uint slot)
{
    auto &binding_set = get_program(program_h).binding_set;
    bind_images_internal(*this, binding_set, slot, image_views_h);
}

// sampled images
void API::bind_combined_image_sampler(GraphicsProgramH program_h, ImageViewH image_view_h, SamplerH sampler_h, uint set, uint slot, uint index)
{
    assert(!(program_h.is_valid() && set == GLOBAL_DESCRIPTOR_SET));
    auto &binding_set = set == GLOBAL_DESCRIPTOR_SET ? global_bindings.binding_set : get_program(program_h).binding_sets_by_freq[set-1];
    bind_image_internal(*this, binding_set, slot, index, image_view_h, sampler_h);
}

void API::bind_combined_image_sampler(ComputeProgramH program_h, ImageViewH image_view_h, SamplerH sampler_h, uint slot, uint index)
{
    auto &binding_set = get_program(program_h).binding_set;
    bind_image_internal(*this, binding_set, slot, index, image_view_h, sampler_h);
}

void API::bind_combined_images_samplers(GraphicsProgramH program_h,
                                        const std::vector<ImageViewH> &image_views_h,
                                        const std::vector<SamplerH> &samplers,
                                        uint set,
                                        uint slot)
{
    assert(!(program_h.is_valid() && set == GLOBAL_DESCRIPTOR_SET));
    auto &binding_set = set == GLOBAL_DESCRIPTOR_SET ? global_bindings.binding_set : get_program(program_h).binding_sets_by_freq[set-1];
    bind_images_internal(*this, binding_set, slot, image_views_h, samplers);
}
void API::bind_combined_images_samplers(ComputeProgramH program_h,
                                        const std::vector<ImageViewH> &image_views_h,
                                        const std::vector<SamplerH> &samplers,
                                        uint slot)
{
    auto &binding_set = get_program(program_h).binding_set;
    bind_images_internal(*this, binding_set, slot, image_views_h, samplers);
}


void API::bind_buffer(GraphicsProgramH program_h, CircularBufferPosition buffer_pos, uint set, uint slot)
{
    assert(!(program_h.is_valid() && set == GLOBAL_DESCRIPTOR_SET));
    auto &buffer  = get_buffer(buffer_pos.buffer_h);
    auto &binding_set = set == GLOBAL_DESCRIPTOR_SET ? global_bindings.binding_set : get_program(program_h).binding_sets_by_freq[set-1];
    bind_buffer_internal(*this, binding_set, buffer, buffer_pos, slot);
}

void API::bind_buffer(ComputeProgramH program_h, CircularBufferPosition buffer_pos, uint slot)
{
    auto &buffer  = get_buffer(buffer_pos.buffer_h);
    auto &binding_set = get_program(program_h).binding_set;
    bind_buffer_internal(*this, binding_set, buffer, buffer_pos, slot);
}


void API::create_global_set()
{
    init_binding_set(ctx, global_bindings.binding_set);
}

void API::update_global_set()
{
    undirty_descriptor_set(*this, global_bindings.binding_set);
}

/// --- Rendering API

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

void API::push_constant(VkShaderStageFlags stage, u32 offset, u32 size, void *data)
{
    assert(current_program);
    auto &frame_resource = ctx.frame_resources.get_current();
    const auto &program  = *current_program;
    vkCmdPushConstants(frame_resource.command_buffer, program.pipeline_layout, stage, offset, size, data);
}

static void pre_draw(API &api, GraphicsProgram &program)
{
    auto &frame_resource = api.ctx.frame_resources.get_current();
    VkCommandBuffer cmd  = frame_resource.command_buffer;

    auto &binding_set = program.binding_sets_by_freq[DRAW_DESCRIPTOR_SET - 1];

    // CHECK early exit
    if (binding_set.bindings_info.empty()) {
        return;
    }

    undirty_descriptor_set(api, binding_set);
    auto &shader_set      = get_descriptor_set(binding_set);
    shader_set.frame_used = api.ctx.frame_count;

    // bind both sets
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            program.pipeline_layout,
                            0,
                            1,
                            &shader_set.set,
                            binding_set.dynamic_offsets.size(),
                            binding_set.dynamic_offsets.data());
}

void API::draw_indexed(u32 index_count, u32 instance_count, u32 first_index, i32 vertex_offset, u32 first_instance)
{
    pre_draw(*this, *current_program);
    auto &frame_resource = ctx.frame_resources.get_current();
    vkCmdDrawIndexed(frame_resource.command_buffer, index_count, instance_count, first_index, vertex_offset, first_instance);
    draws_this_frame++;
}

void API::draw(u32 vertex_count, u32 instance_count, u32 first_vertex, u32 first_instance)
{
    pre_draw(*this, *current_program);
    auto &frame_resource = ctx.frame_resources.get_current();
    vkCmdDraw(frame_resource.command_buffer, vertex_count, instance_count, first_vertex, first_instance);
    draws_this_frame++;
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
    assert(!name.empty());
    assert(current_label.empty());

    auto &frame_resource = ctx.frame_resources.get_current();
    VkDebugUtilsLabelEXT info = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
    info.pLabelName           = name.data();
    info.color[0]             = color.x;
    info.color[1]             = color.y;
    info.color[2]             = color.z;
    info.color[3]             = color.w;
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

void API::dispatch(ComputeProgramH program_h, u32 x, u32 y, u32 z)
{
    auto &program        = get_program(program_h);
    auto &frame_resource = ctx.frame_resources.get_current();
    auto &cmd             = frame_resource.command_buffer;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, program.pipeline_vk);

    /// --- Find and bind descriptor set
    undirty_descriptor_set(*this, program.binding_set);
    auto &descriptor_set      = get_descriptor_set(program.binding_set);
    descriptor_set.frame_used = ctx.frame_count;

    auto &global_set      = get_descriptor_set(global_bindings.binding_set);
    global_set.frame_used = ctx.frame_count;

    std::array descriptor_sets = {global_set.set, descriptor_set.set};

    auto dynamic_offsets = global_bindings.binding_set.dynamic_offsets;
    dynamic_offsets.insert(dynamic_offsets.end(), program.binding_set.dynamic_offsets.begin(), program.binding_set.dynamic_offsets.end());

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, program.pipeline_layout, 0, descriptor_sets.size(), descriptor_sets.data(), dynamic_offsets.size(), dynamic_offsets.data());


    vkCmdDispatch(cmd, x, y, z);
}
} // namespace my_app::vulkan
