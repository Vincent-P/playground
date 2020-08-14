#include "renderer/hl_api.hpp"
#include "renderer/vlk_context.hpp"
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>

namespace my_app::vulkan
{

// TODO: multiple render targets, multisampling
static RenderPassH find_or_create_render_pass(API &api, PassInfo &&info)
{
    for (auto it = api.renderpasses.begin(); it != api.renderpasses.end(); ++it) {
        const auto &render_pass = *it;
        if (render_pass.info == info) {
            return RenderPassH(it.index());
        }
    }

    RenderPass rp;
    rp.info = std::move(info);

    std::vector<vk::AttachmentDescription> attachments;

    vk::AttachmentReference color_ref(0, vk::ImageLayout::eColorAttachmentOptimal);

    bool separateDepthStencilLayouts = api.ctx.vulkan12_features.separateDepthStencilLayouts;
    vk::AttachmentReference depth_ref(0, separateDepthStencilLayouts ? vk::ImageLayout::eDepthAttachmentOptimal : vk::ImageLayout::eGeneral);

    if (rp.info.color) {
        color_ref.attachment = static_cast<u32>(attachments.size());

        vk::ImageLayout initial_layout;
        auto final_layout = rp.info.present ? vk::ImageLayout::ePresentSrcKHR : vk::ImageLayout::eShaderReadOnlyOptimal;
        vk::Format format;

        auto color_rt  = api.get_rendertarget(rp.info.color->rt);
        if (color_rt.is_swapchain)
        {
            initial_layout = rp.info.color->load_op == vk::AttachmentLoadOp::eClear ? vk::ImageLayout::eUndefined : vk::ImageLayout::ePresentSrcKHR;
            format = api.ctx.swapchain.format.format;
        }
        else
        {
            auto &color_img = api.get_image(color_rt.image_h);
            initial_layout = color_img.layout;
            format = color_img.image_info.format;

            if (final_layout != color_img.layout)
            {
                color_img.layout = final_layout;
                color_img.access = access_from_layout(color_img.layout);
            }
        }

        if (initial_layout == vk::ImageLayout::eUndefined)
        {
            rp.info.color->load_op = vk::AttachmentLoadOp::eClear;
        }


        vk::AttachmentDescription attachment;
        attachment.format         = format;
        attachment.samples        = vk::SampleCountFlagBits::e1;
        attachment.loadOp         = rp.info.color->load_op;
        attachment.storeOp        = vk::AttachmentStoreOp::eStore;
        attachment.stencilLoadOp  = vk::AttachmentLoadOp::eDontCare;
        attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        attachment.initialLayout  = initial_layout;
        attachment.finalLayout    = final_layout;
        attachment.flags          = {};
        attachments.push_back(std::move(attachment));
    }

    if (rp.info.depth) {
        depth_ref.attachment = static_cast<u32>(attachments.size());

        const auto &depth_rt    = api.get_rendertarget(rp.info.depth->rt);
        const auto &depth_image = api.get_image(depth_rt.image_h);

        vk::AttachmentDescription attachment;
        attachment.format         = depth_image.image_info.format;
        attachment.samples        = vk::SampleCountFlagBits::e1;
        attachment.loadOp         = rp.info.depth->load_op;
        attachment.storeOp        = vk::AttachmentStoreOp::eStore;
        attachment.stencilLoadOp  = vk::AttachmentLoadOp::eDontCare;
        attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        attachment.initialLayout  = rp.info.depth->load_op == vk::AttachmentLoadOp::eClear ? vk::ImageLayout::eUndefined : vk::ImageLayout::eDepthStencilReadOnlyOptimal;
        attachment.finalLayout    = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
        attachment.flags          = {};
        attachments.push_back(std::move(attachment));
    }

    std::array<vk::SubpassDescription, 1> subpasses{};
    subpasses[0].flags                = vk::SubpassDescriptionFlags(0);
    subpasses[0].pipelineBindPoint    = vk::PipelineBindPoint::eGraphics;
    subpasses[0].inputAttachmentCount = 0;
    subpasses[0].pInputAttachments    = nullptr;
    subpasses[0].colorAttachmentCount = rp.info.color ? 1 : 0;
    subpasses[0].pColorAttachments    = rp.info.color ? &color_ref : nullptr;
    subpasses[0].pResolveAttachments  = nullptr;

    subpasses[0].pDepthStencilAttachment = rp.info.depth ? &depth_ref : nullptr;

    subpasses[0].preserveAttachmentCount = 0;
    subpasses[0].pPreserveAttachments    = nullptr;

    std::array<vk::SubpassDependency, 0> dependencies{};

    vk::RenderPassCreateInfo rp_info{};
    rp_info.attachmentCount = static_cast<u32>(attachments.size());
    rp_info.pAttachments    = attachments.data();
    rp_info.subpassCount    = subpasses.size();
    rp_info.pSubpasses      = subpasses.data();
    rp_info.dependencyCount = dependencies.size();
    rp_info.pDependencies   = dependencies.data();
    rp.vkhandle             = api.ctx.device->createRenderPassUnique(rp_info);

    return api.renderpasses.add(std::move(rp));
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

    std::vector<vk::ImageView> attachments;
    if (render_pass.info.color) {
        attachments.push_back(fb.info.image_view);
    }

    if (render_pass.info.depth) {
        attachments.push_back(fb.info.depth_view);
    }

    vk::FramebufferCreateInfo ci{};
    ci.renderPass      = *render_pass.vkhandle;
    ci.attachmentCount = static_cast<u32>(attachments.size());
    ci.pAttachments    = attachments.data();
    ci.layers          = 1;
    ci.width           = fb.info.width;
    ci.height          = fb.info.height;
    fb.vkhandle        = api.ctx.device->createFramebufferUnique(ci);

    api.framebuffers.push_back(std::move(fb));
    return api.framebuffers.back();
}

void API::begin_pass(PassInfo &&info)
{
    auto render_pass_h = find_or_create_render_pass(*this, std::move(info));
    auto &render_pass = *renderpasses.get(render_pass_h);

    FrameBufferInfo fb_info;

    if (render_pass.info.color) {
        bool is_swapchain = get_rendertarget(render_pass.info.color->rt).is_swapchain;
        if (!is_swapchain) {
            const auto &rt     = get_rendertarget(render_pass.info.color->rt);
            const auto &image  = get_image(rt.image_h);
            fb_info.image_view = image.default_view;
        }
        else {
            fb_info.image_view = ctx.swapchain.get_current_image_view();
        }
    }

    if (render_pass.info.depth) {
        const auto &depth_rt    = get_rendertarget(render_pass.info.depth->rt);
        const auto &depth_image = get_image(depth_rt.image_h);
        fb_info.depth_view      = depth_image.default_view;
    }

    fb_info.render_pass = *render_pass.vkhandle;

    if (render_pass.info.color) {
        const auto &rt = get_rendertarget(render_pass.info.color->rt);
        if (rt.is_swapchain) {
            fb_info.width  = ctx.swapchain.extent.width;
            fb_info.height = ctx.swapchain.extent.height;
        }
        else {
            const auto &image = get_image(rt.image_h);
            fb_info.width     = image.image_info.extent.width;
            fb_info.height    = image.image_info.extent.height;
        }
    }
    else if (render_pass.info.depth) {
        const auto &rt    = get_rendertarget(render_pass.info.depth->rt);
        const auto &image = get_image(rt.image_h);
        fb_info.width     = image.image_info.extent.width;
        fb_info.height    = image.image_info.extent.height;
    }
    else {
        fb_info.width  = 4096;
        fb_info.height = 4096;
    }

    auto &frame_buffer = find_or_create_frame_buffer(*this, fb_info, render_pass);

    auto &frame_resource = ctx.frame_resources.get_current();

    vk::Rect2D render_area{vk::Offset2D(), {frame_buffer.info.width, frame_buffer.info.height}};

    std::vector<vk::ClearValue> clear_values;

    if (render_pass.info.color && render_pass.info.color->load_op == vk::AttachmentLoadOp::eClear) {
        vk::ClearValue clear;
        clear.color = vk::ClearColorValue(std::array<float, 4>{0.6f, 0.7f, 0.94f, 1.0f});
        clear_values.push_back(std::move(clear));
    }

    if (render_pass.info.depth && render_pass.info.depth->load_op == vk::AttachmentLoadOp::eClear) {
        if (render_pass.info.color && clear_values.empty()) {
            vk::ClearValue clear;
            clear.color = vk::ClearColorValue(std::array<float, 4>{0.6f, 0.7f, 0.94f, 1.0f});
            clear_values.push_back(std::move(clear));
        }
        vk::ClearValue clear;
        clear.depthStencil = vk::ClearDepthStencilValue(1.0f, 0);
        clear_values.push_back(std::move(clear));
    }

    vk::RenderPassBeginInfo rpbi{};
    rpbi.renderArea      = render_area;
    rpbi.renderPass      = *render_pass.vkhandle;
    rpbi.framebuffer     = *frame_buffer.vkhandle;
    rpbi.clearValueCount = static_cast<u32>(clear_values.size());
    rpbi.pClearValues    = clear_values.data();

    current_render_pass = render_pass_h;

    frame_resource.command_buffer->beginRenderPass(rpbi, vk::SubpassContents::eInline);
}

void API::end_pass()
{
    auto &frame_resource = ctx.frame_resources.get_current();
    frame_resource.command_buffer->endRenderPass();

    auto *maybe_render_pass = renderpasses.get(current_render_pass);
    assert(maybe_render_pass);
    auto &render_pass = *maybe_render_pass;

    if (render_pass.info.depth) {
        auto &depth_rt     = get_rendertarget(render_pass.info.depth->rt);
        auto &depth_image  = get_image(depth_rt.image_h);
        depth_image.layout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
    }

    current_render_pass = RenderPassH::invalid();
}

static vk::Pipeline find_or_create_pipeline(API &api, GraphicsProgram &program, PipelineInfo &pipeline_info)
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
        std::vector<vk::DynamicState> dynamic_states = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};

        vk::PipelineDynamicStateCreateInfo dyn_i{{}, static_cast<u32>(dynamic_states.size()), dynamic_states.data()};

        const auto &program_info       = pipeline_info.program_info;
        const auto &vertex_buffer_info = program_info.vertex_buffer_info;
        const auto &render_pass        = *api.renderpasses.get(pipeline_info.render_pass);

        // TODO: support 0 or more than 1 vertex buffer
        // but full screen triangle already works without vertex buffer?
        std::array<vk::VertexInputBindingDescription, 1> bindings;
        bindings[0].binding   = 0;
        bindings[0].stride    = vertex_buffer_info.stride;
        bindings[0].inputRate = vk::VertexInputRate::eVertex;

        std::vector<vk::VertexInputAttributeDescription> attributes;
        attributes.reserve(vertex_buffer_info.vertices_info.size());

        u32 location = 0;
        for (const auto &vertex_info : vertex_buffer_info.vertices_info) {
            vk::VertexInputAttributeDescription attribute;
            attribute.binding  = bindings[0].binding;
            attribute.location = location;
            attribute.format   = vertex_info.format;
            attribute.offset   = vertex_info.offset;
            attributes.push_back(std::move(attribute));
            location++;
        }

        vk::PipelineVertexInputStateCreateInfo vert_i{};
        vert_i.vertexBindingDescriptionCount   = bindings.size();
        vert_i.pVertexBindingDescriptions      = bindings.data();
        vert_i.vertexAttributeDescriptionCount = static_cast<u32>(attributes.size());
        vert_i.pVertexAttributeDescriptions    = attributes.data();

        vk::PipelineInputAssemblyStateCreateInfo asm_i{};
        asm_i.flags                  = {};
        asm_i.primitiveRestartEnable = VK_FALSE;
        asm_i.topology               = vk::PrimitiveTopology::eTriangleList;


        vk::PipelineRasterizationConservativeStateCreateInfoEXT conservative{};
        vk::PipelineRasterizationStateCreateInfo rast_i{};

        if (program_info.enable_conservative_rasterization)
        {
            rast_i.pNext = &conservative;
            conservative.conservativeRasterizationMode = vk::ConservativeRasterizationModeEXT::eOverestimate;
            conservative.extraPrimitiveOverestimationSize = 0.1; // in pixels
        }

        rast_i.flags                   = {};
        rast_i.polygonMode             = vk::PolygonMode::eFill;
        rast_i.cullMode                = vk::CullModeFlagBits::eNone;
        rast_i.frontFace               = vk::FrontFace::eCounterClockwise;
        rast_i.depthClampEnable        = VK_FALSE;
        rast_i.rasterizerDiscardEnable = VK_FALSE;
        rast_i.depthBiasEnable         = VK_FALSE;
        rast_i.depthBiasConstantFactor = 0;
        rast_i.depthBiasClamp          = 0;
        rast_i.depthBiasSlopeFactor    = 0;
        rast_i.lineWidth               = 1.0f;

        // TODO: from render_pass
        std::array<vk::PipelineColorBlendAttachmentState, 1> att_states;
        att_states[0].colorWriteMask      = {vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};
        att_states[0].blendEnable         = VK_TRUE;
        att_states[0].srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
        att_states[0].dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
        att_states[0].colorBlendOp        = vk::BlendOp::eAdd;
        att_states[0].srcAlphaBlendFactor = vk::BlendFactor::eOne;
        att_states[0].dstAlphaBlendFactor = vk::BlendFactor::eZero;
        att_states[0].alphaBlendOp        = vk::BlendOp::eAdd;

        if (render_pass.info.color)
        {
            auto &color_attachment = *render_pass.info.color;
            auto &color = api.get_rendertarget(color_attachment.rt);
            if (!color.is_swapchain)
            {
                auto &image = api.get_image(color.image_h);
                // TODO: disable for all uint
                if (image.image_info.format == vk::Format::eR8Uint) {
                    att_states[0].blendEnable = VK_FALSE;
                }
            }
        }


        vk::PipelineColorBlendStateCreateInfo colorblend_i{};
        colorblend_i.flags             = {};
        colorblend_i.attachmentCount   = att_states.size();
        colorblend_i.pAttachments      = att_states.data();
        colorblend_i.logicOpEnable     = VK_FALSE;
        colorblend_i.logicOp           = vk::LogicOp::eCopy;
        colorblend_i.blendConstants[0] = 0.0f;
        colorblend_i.blendConstants[1] = 0.0f;
        colorblend_i.blendConstants[2] = 0.0f;
        colorblend_i.blendConstants[3] = 0.0f;

        vk::PipelineViewportStateCreateInfo vp_i{};
        vp_i.flags         = {};
        vp_i.viewportCount = 1;
        vp_i.scissorCount  = 1;
        vp_i.pScissors     = nullptr;
        vp_i.pViewports    = nullptr;

        vk::PipelineDepthStencilStateCreateInfo ds_i{};
        ds_i.flags = vk::PipelineDepthStencilStateCreateFlags();

        ds_i.depthTestEnable       = pipeline_info.program_info.depth_test ? VK_TRUE : VK_FALSE;
        ds_i.depthWriteEnable      = pipeline_info.program_info.enable_depth_write ? VK_TRUE : VK_FALSE;
        ds_i.depthCompareOp        = pipeline_info.program_info.depth_test ? *pipeline_info.program_info.depth_test : vk::CompareOp::eLessOrEqual;
        ds_i.depthBoundsTestEnable = VK_FALSE;
        ds_i.minDepthBounds        = 0.0f;
        ds_i.maxDepthBounds        = 0.0f;
        ds_i.stencilTestEnable     = VK_FALSE;
        ds_i.back.failOp           = vk::StencilOp::eKeep;
        ds_i.back.passOp           = vk::StencilOp::eKeep;
        ds_i.back.compareOp        = vk::CompareOp::eAlways;
        ds_i.back.compareMask      = 0;
        ds_i.back.reference        = 0;
        ds_i.back.depthFailOp      = vk::StencilOp::eKeep;
        ds_i.back.writeMask        = 0;
        ds_i.front                 = ds_i.back;

        vk::PipelineMultisampleStateCreateInfo ms_i{};
        ms_i.flags                 = {};
        ms_i.pSampleMask           = nullptr;
        ms_i.rasterizationSamples  = render_pass.info.samples;
        ms_i.sampleShadingEnable   = VK_FALSE;
        ms_i.alphaToCoverageEnable = VK_FALSE;
        ms_i.alphaToOneEnable      = VK_FALSE;
        ms_i.minSampleShading      = .2f;

        std::vector<vk::PipelineShaderStageCreateInfo> shader_stages;
        shader_stages.reserve(3);

        if (program_info.vertex_shader.is_valid()) {
            const auto &shader = api.get_shader(program_info.vertex_shader);
            vk::PipelineShaderStageCreateInfo create_info{{}, vk::ShaderStageFlagBits::eVertex, *shader.vkhandle, "main"};
            shader_stages.push_back(std::move(create_info));
        }

        if (program_info.geom_shader.is_valid()) {
            const auto &shader = api.get_shader(program_info.geom_shader);
            vk::PipelineShaderStageCreateInfo create_info{{}, vk::ShaderStageFlagBits::eGeometry, *shader.vkhandle, "main"};
            shader_stages.push_back(std::move(create_info));
        }

        if (program_info.fragment_shader.is_valid()) {
            const auto &shader = api.get_shader(program_info.fragment_shader);
            vk::PipelineShaderStageCreateInfo create_info{{}, vk::ShaderStageFlagBits::eFragment, *shader.vkhandle, "main"};
            shader_stages.push_back(std::move(create_info));
        }

        vk::GraphicsPipelineCreateInfo pipe_i{};
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
        pipe_i.renderPass          = *render_pass.vkhandle;
        pipe_i.subpass             = 0; // TODO: subpasses

        program.pipelines_info.push_back(pipeline_info);
        program.pipelines_vk.push_back(api.ctx.device->createGraphicsPipelineUnique(nullptr, pipe_i));
        pipeline_i = static_cast<u32>(program.pipelines_vk.size()) - 1;
    }

    return *program.pipelines_vk[pipeline_i];
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

    vk::DescriptorSetAllocateInfo dsai{};
    dsai.descriptorPool     = *api.ctx.descriptor_pool;
    dsai.pSetLayouts        = &program.descriptor_layouts[freq].get();
    dsai.descriptorSetCount = 1;
    api.ctx.descriptor_sets_count++;

    descriptor_set.set        = std::move(api.ctx.device->allocateDescriptorSets(dsai)[0]);
    descriptor_set.frame_used = api.ctx.frame_count;

    program.descriptor_sets[freq].push_back(std::move(descriptor_set));

    program.current_descriptor_set[freq] = program.descriptor_sets[freq].size() - 1;

    return program.descriptor_sets[freq].back();
}

static void undirty_descriptor_set(API &api, GraphicsProgram &program, uint i_set)
{
    if (program.data_dirty_by_set[i_set]) {
        auto &descriptor_set = find_or_create_descriptor_set(api, program, i_set);

        std::vector<vk::WriteDescriptorSet> writes;
        map_transform(program.binded_data_by_set[i_set], writes, [&](const auto &binded_data) {
            assert(binded_data.has_value());
            vk::WriteDescriptorSet write;
            write.dstSet           = descriptor_set.set;
            write.dstBinding       = binded_data->binding;
            write.descriptorCount  = binded_data->images_info.empty() ? 1 : binded_data->images_info.size();
            write.descriptorType   = binded_data->type;
            write.pImageInfo       = binded_data->images_info.data();
            write.pBufferInfo      = &binded_data->buffer_info;
            write.pTexelBufferView = &binded_data->buffer_view;
            return write;
        });

        api.ctx.device->updateDescriptorSets(writes, nullptr);

        program.data_dirty_by_set[i_set] = false;
    }
}

static void bind_descriptor_set(API &api, GraphicsProgram &program, uint i_set)
{
    auto &frame_resource = api.ctx.frame_resources.get_current();
    auto cmd             = *frame_resource.command_buffer;

    /// --- Find and bind descriptor set
    undirty_descriptor_set(api, program, i_set);
    auto &descriptor_set      = program.descriptor_sets[i_set][program.current_descriptor_set[i_set]];
    descriptor_set.frame_used = api.ctx.frame_count;

    std::vector<u32> offsets;
    offsets.resize(program.dynamic_count_by_set[i_set]);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *program.pipeline_layout, i_set, descriptor_set.set, offsets);
}

void API::bind_program(GraphicsProgramH H)
{
    assert(current_render_pass.is_valid());
    auto &frame_resource = ctx.frame_resources.get_current();
    auto &program        = get_program(H);

    /// --- Find and bind graphics pipeline
    PipelineInfo pipeline_info{program.info, *program.pipeline_layout, current_render_pass};
    auto pipeline = find_or_create_pipeline(*this, program, pipeline_info);
    frame_resource.command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);

    bind_descriptor_set(*this, program, GLOBAL_DESCRIPTOR_SET);
    bind_descriptor_set(*this, program, SHADER_DESCRIPTOR_SET);

    current_program = &program;
}

static void bind_image_internal(API &api, const std::vector<ImageH> &images_h, const std::vector<vk::ImageView> &images_view, std::vector<std::optional<ShaderBinding>> &binded_data, std::vector<BindingInfo> &bindings, bool &data_dirty, uint slot)
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
        auto &image = api.get_image(images_h[i]);
        const auto &image_view = images_view[i];

        assert(image.layout == vk::ImageLayout::eShaderReadOnlyOptimal || image.layout == vk::ImageLayout::eDepthStencilReadOnlyOptimal || image.layout == vk::ImageLayout::eGeneral);

        data.images_info.push_back({});
        auto &image_info = data.images_info.back();

        image_info.imageView   = image_view;
        image_info.sampler     = image.default_sampler;
        image_info.imageLayout = image.layout;
    }

    if (!binded_data[slot].has_value() || *binded_data[slot] != data) {
        binded_data[slot] = std::move(data);
        data_dirty        = true;
    }
}

void API::bind_image(GraphicsProgramH program_h, uint set, uint slot, ImageH image_h, std::optional<vk::ImageView> image_view)
{
    auto &program = get_program(program_h);
    auto view = image_view ? *image_view : get_image(image_h).default_view;
    bind_image_internal(*this, {image_h}, {view}, program.binded_data_by_set[set], program.info.bindings_by_set[set], program.data_dirty_by_set[set], slot);
}

void API::bind_image(ComputeProgramH program_h, uint slot, ImageH image_h, std::optional<vk::ImageView> image_view)
{
    auto &program = get_program(program_h);
    auto view = image_view ? *image_view : get_image(image_h).default_view;
    bind_image_internal(*this, {image_h}, {view}, program.binded_data, program.info.bindings, program.data_dirty, slot);
}

void API::bind_images(GraphicsProgramH program_h, uint set, uint slot, const std::vector<ImageH> &images_h, const std::vector<vk::ImageView> &images_view)
{
    auto &program = get_program(program_h);
    bind_image_internal(*this, images_h, images_view, program.binded_data_by_set[set], program.info.bindings_by_set[set], program.data_dirty_by_set[set], slot);
}

void API::bind_images(ComputeProgramH program_h, uint slot, const std::vector<ImageH> &images_h, const std::vector<vk::ImageView> &images_view)
{
    auto &program = get_program(program_h);
    bind_image_internal(*this, images_h, images_view, program.binded_data, program.info.bindings, program.data_dirty, slot);
}

static void bind_combined_image_sampler_internal(API& api, const std::vector<ImageH> &images_h, const std::vector<vk::ImageView> images_view, Sampler &sampler, std::vector<std::optional<ShaderBinding>> &binded_data, std::vector<BindingInfo> &bindings, bool &data_dirty, uint slot)
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
        auto &image = api.get_image(images_h[i]);
        const auto &image_view = images_view[i];

        data.images_info.push_back({});
        auto &image_info = data.images_info.back();

        image_info.imageView   = image_view;
        image_info.sampler     = *sampler.vkhandle;
        image_info.imageLayout = image.layout;

        // TODO: improve layout management
        if (image_info.imageLayout != vk::ImageLayout::eShaderReadOnlyOptimal && image_info.imageLayout != vk::ImageLayout::eDepthStencilReadOnlyOptimal) {
            image_info.imageLayout = vk::ImageLayout::eGeneral;
        }
    }

    if (!binded_data[slot].has_value() || *binded_data[slot] != data) {
        binded_data[slot] = std::move(data);
        data_dirty        = true;
    }
}

void API::bind_combined_image_sampler(GraphicsProgramH program_h, uint set, uint slot, ImageH image_h, SamplerH sampler_h, std::optional<vk::ImageView> image_view)
{
    auto &program = get_program(program_h);
    auto &sampler = get_sampler(sampler_h);
    auto view = image_view ? *image_view : get_image(image_h).default_view;
    bind_combined_image_sampler_internal(*this, {image_h}, {view}, sampler, program.binded_data_by_set[set], program.info.bindings_by_set[set], program.data_dirty_by_set[set], slot);
}


void API::bind_combined_image_sampler(ComputeProgramH program_h, uint slot, ImageH image_h, SamplerH sampler_h, std::optional<vk::ImageView> image_view)
{
    auto &program = get_program(program_h);
    auto &sampler = get_sampler(sampler_h);
    auto view = image_view ? *image_view : get_image(image_h).default_view;
    bind_combined_image_sampler_internal(*this, {image_h}, {view}, sampler, program.binded_data, program.info.bindings, program.data_dirty, slot);
}


void API::bind_combined_images_sampler(GraphicsProgramH program_h, uint set, uint slot, const std::vector<ImageH> &images_h, SamplerH sampler_h, const std::vector<vk::ImageView> &images_view)
{
    auto &program = get_program(program_h);
    auto &sampler = get_sampler(sampler_h);
    bind_combined_image_sampler_internal(*this, images_h, images_view, sampler, program.binded_data_by_set[set], program.info.bindings_by_set[set], program.data_dirty_by_set[set], slot);
}

void API::bind_combined_images_sampler(ComputeProgramH program_h, uint slot, const std::vector<ImageH> &images_h, SamplerH sampler_h, const std::vector<vk::ImageView> &images_view)
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
    frame_resource.command_buffer->bindVertexBuffers(0, {vertex_buffer.vkhandle}, {offset});
}

void API::bind_vertex_buffer(CircularBufferPosition v_pos)
{
    const auto &vertex_buffer = get_buffer(v_pos.buffer_h);
    auto &frame_resource      = ctx.frame_resources.get_current();
    frame_resource.command_buffer->bindVertexBuffers(0, {vertex_buffer.vkhandle}, {v_pos.offset});
}

void API::bind_index_buffer(BufferH H, u32 offset)
{
    const auto &index_buffer = get_buffer(H);
    auto &frame_resource     = ctx.frame_resources.get_current();
    frame_resource.command_buffer->bindIndexBuffer(index_buffer.vkhandle, offset, vk::IndexType::eUint16);
}

void API::bind_index_buffer(CircularBufferPosition i_pos)
{
    const auto &index_buffer = get_buffer(i_pos.buffer_h);
    auto &frame_resource     = ctx.frame_resources.get_current();
    frame_resource.command_buffer->bindIndexBuffer(index_buffer.vkhandle, i_pos.offset, vk::IndexType::eUint16);
}

void API::push_constant(vk::ShaderStageFlagBits stage, u32 offset, u32 size, void *data)
{
    assert(current_program);
    auto &frame_resource = ctx.frame_resources.get_current();
    const auto &program  = *current_program;
    frame_resource.command_buffer->pushConstants(*program.pipeline_layout, stage, offset, size, data);
}

static void pre_draw(API &api, GraphicsProgram &program)
{
    bind_descriptor_set(api, program, DRAW_DESCRIPTOR_SET);
}

void API::draw_indexed(u32 index_count, u32 instance_count, u32 first_index, i32 vertex_offset, u32 first_instance)
{
    pre_draw(*this, *current_program);
    auto &frame_resource = ctx.frame_resources.get_current();
    frame_resource.command_buffer->drawIndexed(index_count, instance_count, first_index, vertex_offset, first_instance);
}

void API::draw(u32 vertex_count, u32 instance_count, u32 first_vertex, u32 first_instance)
{
    pre_draw(*this, *current_program);
    auto &frame_resource = ctx.frame_resources.get_current();
    frame_resource.command_buffer->draw(vertex_count, instance_count, first_vertex, first_instance);
}

void API::set_scissor(const vk::Rect2D &scissor)
{
    auto &frame_resource = ctx.frame_resources.get_current();
    frame_resource.command_buffer->setScissor(0, scissor);
}

void API::set_viewport(const vk::Viewport &viewport)
{
    auto &frame_resource = ctx.frame_resources.get_current();
    frame_resource.command_buffer->setViewport(0, viewport);
}

void API::begin_label(std::string_view name, float4 color)
{
    auto &frame_resource = ctx.frame_resources.get_current();
    vk::DebugUtilsLabelEXT info{};
    info.pLabelName = name.data();
    info.color[0] = color[0];
    info.color[1] = color[1];
    info.color[2] = color[2];
    info.color[3] = color[3];
    frame_resource.command_buffer->beginDebugUtilsLabelEXT(&info);

    add_gpu_timestamp(name);
}

void API::add_gpu_timestamp(std::string_view label)
{
    auto &frame_resource = ctx.frame_resources.get_current();
    u32 frame_idx = ctx.frame_count % FRAMES_IN_FLIGHT;
    u32 offset    = frame_idx * MAX_TIMESTAMP_PER_FRAME + current_timestamp_labels.size();

    frame_resource.command_buffer->writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, *ctx.timestamp_pool, offset);

    current_timestamp_labels.push_back(label);
}

void API::end_label()
{
    auto &frame_resource = ctx.frame_resources.get_current();
    frame_resource.command_buffer->endDebugUtilsLabelEXT();
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

    vk::DescriptorSetAllocateInfo dsai{};
    dsai.descriptorPool     = *api.ctx.descriptor_pool;
    dsai.pSetLayouts        = &program.descriptor_layout.get();
    dsai.descriptorSetCount = 1;

    descriptor_set.set        = std::move(api.ctx.device->allocateDescriptorSets(dsai)[0]);
    descriptor_set.frame_used = api.ctx.frame_count;

    program.descriptor_sets.push_back(std::move(descriptor_set));

    program.current_descriptor_set = program.descriptor_sets.size() - 1;

    return program.descriptor_sets.back();
}

void API::dispatch(ComputeProgramH program_h, u32 x, u32 y, u32 z)
{
    auto &program        = get_program(program_h);
    auto &frame_resource = ctx.frame_resources.get_current();
    auto &cmd            = *frame_resource.command_buffer;

    const auto &compute_shader = get_shader(program.info.shader);

    vk::ComputePipelineCreateInfo pinfo{};
    pinfo.stage.stage  = vk::ShaderStageFlagBits::eCompute;
    pinfo.stage.module = *compute_shader.vkhandle;
    pinfo.stage.pName  = "main";
    pinfo.layout       = *program.pipeline_layout;

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
        pipeline = ctx.device->createComputePipelineUnique(nullptr, pinfo);
    }


    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *program.pipelines_vk[pipeline_i]);

    /// --- Find and bind descriptor set
    if (program.data_dirty) {
        auto &descriptor_set = find_or_create_descriptor_set(*this, program);

        std::vector<vk::WriteDescriptorSet> writes;

        for (const auto &binded_data : program.binded_data)
        {
            assert(binded_data.has_value());

            writes.emplace_back();
            vk::WriteDescriptorSet &write = writes.back();
            write.dstSet           = descriptor_set.set;
            write.dstBinding       = binded_data->binding;
            write.descriptorCount  = binded_data->images_info.empty() ? 1 : binded_data->images_info.size();
            write.descriptorType   = binded_data->type;
            write.pImageInfo       = binded_data->images_info.data();
            write.pBufferInfo      = &binded_data->buffer_info;
            write.pTexelBufferView = &binded_data->buffer_view;
        }

        ctx.device->updateDescriptorSets(writes, nullptr);

        program.data_dirty = false;
    }

    auto &descriptor_set      = program.descriptor_sets[program.current_descriptor_set];
    descriptor_set.frame_used = ctx.frame_count;

    std::vector<u32> offsets;
    offsets.resize(program.dynamic_count);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *program.pipeline_layout, 0, descriptor_set.set, offsets);

    cmd.dispatch(x, y, z);
}
} // namespace my_app::vulkan
