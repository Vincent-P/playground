#include "renderer/hl_api.hpp"
#include <vulkan/vulkan.hpp>

namespace my_app::vulkan
{

// TODO: multiple render targets, multisampling, depth
static RenderPass &find_or_create_render_pass(API &api, PassInfo &&info)
{
    assert(api.get_rendertarget(info.attachment.rt).is_swapchain);

    for (auto &render_pass : api.renderpasses) {
        if (render_pass.info == info) {
            return render_pass;
        }
    }

    RenderPass rp;
    rp.info = std::move(info);

    auto final_layout = rp.info.present ? vk::ImageLayout::ePresentSrcKHR : vk::ImageLayout::eShaderReadOnlyOptimal;

    std::array<vk::AttachmentDescription, 1> attachments;
    attachments[0].format         = api.ctx.swapchain.format.format;
    attachments[0].samples        = vk::SampleCountFlagBits::e1;
    attachments[0].loadOp         = rp.info.attachment.load_op;
    attachments[0].storeOp        = vk::AttachmentStoreOp::eStore;
    attachments[0].stencilLoadOp  = vk::AttachmentLoadOp::eDontCare;
    attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    attachments[0].initialLayout  = vk::ImageLayout::eUndefined;
    attachments[0].finalLayout    = final_layout;
    attachments[0].flags          = {};

    vk::AttachmentReference color_ref(0, vk::ImageLayout::eColorAttachmentOptimal);

    std::array<vk::SubpassDescription, 1> subpasses{};
    subpasses[0].flags                   = vk::SubpassDescriptionFlags(0);
    subpasses[0].pipelineBindPoint       = vk::PipelineBindPoint::eGraphics;
    subpasses[0].inputAttachmentCount    = 0;
    subpasses[0].pInputAttachments       = nullptr;
    subpasses[0].colorAttachmentCount    = 1;
    subpasses[0].pColorAttachments       = &color_ref;
    subpasses[0].pResolveAttachments     = nullptr;
    subpasses[0].pDepthStencilAttachment = nullptr;
    subpasses[0].preserveAttachmentCount = 0;
    subpasses[0].pPreserveAttachments    = nullptr;

    std::array<vk::SubpassDependency, 0> dependencies{};

    vk::RenderPassCreateInfo rp_info{};
    rp_info.attachmentCount = attachments.size();
    rp_info.pAttachments    = attachments.data();
    rp_info.subpassCount    = subpasses.size();
    rp_info.pSubpasses      = subpasses.data();
    rp_info.dependencyCount = dependencies.size();
    rp_info.pDependencies   = dependencies.data();
    rp.vkhandle             = api.ctx.device->createRenderPassUnique(rp_info);

    api.renderpasses.push_back(std::move(rp));
    return api.renderpasses.back();
}

// TODO: render targets other than one swapchain image
static FrameBuffer &find_or_create_frame_buffer(API &api, const PassInfo &info, const RenderPass &render_pass)
{
    assert(api.get_rendertarget(info.attachment.rt).is_swapchain);

    FrameBufferInfo fb_info;
    fb_info.image_view  = api.ctx.swapchain.get_current_image_view();
    fb_info.render_pass = *render_pass.vkhandle;

    for (auto &framebuffer : api.framebuffers) {
        if (framebuffer.info == fb_info) {
            return framebuffer;
        }
    }

    FrameBuffer fb;
    fb.info                                  = fb_info;
    std::array<vk::ImageView, 1> attachments = {api.ctx.swapchain.get_current_image_view()};

    vk::FramebufferCreateInfo ci{};
    ci.renderPass      = *render_pass.vkhandle;
    ci.attachmentCount = attachments.size();
    ci.pAttachments    = attachments.data();
    ci.width           = api.ctx.swapchain.extent.width;
    ci.height          = api.ctx.swapchain.extent.height;
    ci.layers          = 1;
    fb.vkhandle        = api.ctx.device->createFramebufferUnique(ci);

    api.framebuffers.push_back(std::move(fb));
    return api.framebuffers.back();
}

void API::begin_pass(PassInfo &&info)
{
    auto &render_pass  = find_or_create_render_pass(*this, std::move(info));
    auto &frame_buffer = find_or_create_frame_buffer(*this, render_pass.info, render_pass);

    auto &frame_resource = ctx.frame_resources.get_current();

    vk::Rect2D render_area{vk::Offset2D(), ctx.swapchain.extent};

    std::array<vk::ClearValue, 3> clear_values;
    clear_values[0].color        = vk::ClearColorValue(std::array<float, 4>{0.6f, 0.7f, 0.94f, 1.0f});
    clear_values[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);
    clear_values[2].color        = vk::ClearColorValue(std::array<float, 4>{0.6f, 0.7f, 0.94f, 1.0f});

    vk::RenderPassBeginInfo rpbi{};
    rpbi.renderArea      = render_area;
    rpbi.renderPass      = *render_pass.vkhandle;
    rpbi.framebuffer     = *frame_buffer.vkhandle;
    rpbi.clearValueCount = render_pass.info.attachment.load_op == vk::AttachmentLoadOp::eClear ? 1 : 0;
    rpbi.pClearValues    = clear_values.data();

    current_render_pass = &render_pass;

    frame_resource.command_buffer->beginRenderPass(rpbi, vk::SubpassContents::eInline);
}

void API::end_pass()
{
    auto &frame_resource = ctx.frame_resources.get_current();
    frame_resource.command_buffer->endRenderPass();
    current_render_pass = nullptr;
}

static vk::Pipeline find_or_create_pipeline(API &api, Program &program, PipelineInfo &&pipeline_info)
{

    u32 pipeline_i = u32_invalid;

    for (u32 i = 0; i < program.pipelines_info.size(); i++) {
        if (program.pipelines_info[i] == pipeline_info) {
            pipeline_i = i;
            break;
        }
    }

    if (pipeline_i == u32_invalid) {
        std::vector<vk::DynamicState> dynamic_states = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};

        vk::PipelineDynamicStateCreateInfo dyn_i{{}, static_cast<u32>(dynamic_states.size()), dynamic_states.data()};

        const auto &program_info       = pipeline_info.program_info;
        const auto &vertex_buffer_info = program_info.vertex_buffer_info;

        // TODO: support 0 or more than 1 vertex buffer
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

        vk::PipelineRasterizationStateCreateInfo rast_i{};
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
        att_states[0].colorWriteMask      = {vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                                        | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};
        att_states[0].blendEnable         = VK_TRUE;
        att_states[0].srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
        att_states[0].dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
        att_states[0].colorBlendOp        = vk::BlendOp::eAdd;
        att_states[0].srcAlphaBlendFactor = vk::BlendFactor::eOne;
        att_states[0].dstAlphaBlendFactor = vk::BlendFactor::eZero;
        att_states[0].alphaBlendOp        = vk::BlendOp::eAdd;

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
        ds_i.flags            = {};
        ds_i.depthTestEnable  = VK_FALSE;
        ds_i.depthWriteEnable = VK_FALSE;
        ds_i.depthCompareOp   = vk::CompareOp::eAlways;

        vk::PipelineMultisampleStateCreateInfo ms_i{};
        ms_i.flags                 = {};
        ms_i.pSampleMask           = nullptr;
        ms_i.rasterizationSamples  = vk::SampleCountFlagBits::e1;
        ms_i.sampleShadingEnable   = VK_FALSE;
        ms_i.alphaToCoverageEnable = VK_FALSE;
        ms_i.alphaToOneEnable      = VK_FALSE;
        ms_i.minSampleShading      = .2f;

        std::vector<vk::PipelineShaderStageCreateInfo> shader_stages;

        if (program_info.vertex_shader.is_valid()) {
            const auto &shader = api.get_shader(program_info.vertex_shader);
            vk::PipelineShaderStageCreateInfo create_info{
                {}, vk::ShaderStageFlagBits::eVertex, *shader.vkhandle, "main"};
            shader_stages.push_back(std::move(create_info));
        }

        if (program_info.fragment_shader.is_valid()) {
            const auto &shader = api.get_shader(program_info.fragment_shader);
            vk::PipelineShaderStageCreateInfo create_info{
                {}, vk::ShaderStageFlagBits::eFragment, *shader.vkhandle, "main"};
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
        pipe_i.renderPass          = pipeline_info.vk_render_pass;
        pipe_i.subpass             = 0; // TODO: subpasses

        program.pipelines_info.push_back(std::move(pipeline_info));
        program.pipelines_vk.push_back(api.ctx.device->createGraphicsPipelineUnique(nullptr, pipe_i));
        pipeline_i = static_cast<u32>(program.pipelines_vk.size()) - 1;
    }

    return *program.pipelines_vk[pipeline_i];
}

static DescriptorSet &find_or_create_descriptor_sets(API &api, Program &program)
{
    for (auto &descriptor_set : program.descriptor_sets) {
        if (descriptor_set.frame_used + api.ctx.frame_resources.data.size() < api.ctx.frame_count) {
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

void API::bind_program(ProgramH H)
{
    assert(current_render_pass != nullptr);
    auto &frame_resource = ctx.frame_resources.get_current();
    auto &program        = get_program(H);
    auto &render_pass    = *current_render_pass;

    /// --- Find and bind graphics pipeline
    PipelineInfo pipeline_info{program.info, *program.pipeline_layout, *render_pass.vkhandle};
    auto pipeline = find_or_create_pipeline(*this, program, std::move(pipeline_info));
    frame_resource.command_buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);

    /// --- TODO: multiple descriptor set, bind shader set here

    current_program = &program;
}

void API::bind_image(ProgramH program_h, uint slot, ImageH image_h)
{
    auto &program = get_program(program_h);
    auto &image   = get_image(image_h);

    if (program.binded_data.size() <= slot) {
        usize missing = slot - program.binded_data.size() + 1;
        for (usize i = 0; i < missing; i++) {
            program.binded_data.emplace_back(std::nullopt);
        }
    }

    ShaderBinding data;
    data.binding                = slot;
    data.type                   = program.info.bindings[slot].type;
    data.image_info.imageView   = image.default_view;
    data.image_info.sampler     = image.default_sampler;
    data.image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    if (!program.binded_data[slot].has_value() || *program.binded_data[slot] != data) {
        program.binded_data[slot] = std::move(data);
        program.data_dirty        = true;
    }
}

void API::bind_vertex_buffer(BufferH H)
{
    const auto &vertex_buffer = get_buffer(H);
    auto &frame_resource      = ctx.frame_resources.get_current();
    frame_resource.command_buffer->bindVertexBuffers(0, {vertex_buffer.vkhandle}, {0});
}

void API::bind_vertex_buffer(CircularBufferPosition v_pos)
{
    const auto &vertex_buffer = get_buffer(v_pos.buffer_h);
    auto &frame_resource      = ctx.frame_resources.get_current();
    frame_resource.command_buffer->bindVertexBuffers(0, {vertex_buffer.vkhandle}, {v_pos.offset});
}

void API::bind_index_buffer(BufferH H)
{
    const auto &index_buffer = get_buffer(H);
    auto &frame_resource     = ctx.frame_resources.get_current();
    frame_resource.command_buffer->bindIndexBuffer(index_buffer.vkhandle, 0, vk::IndexType::eUint16);
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

static void pre_draw(API &api, Program &program)
{
    auto &frame_resource = api.ctx.frame_resources.get_current();
    auto cmd             = *frame_resource.command_buffer;

    /// --- Find and bind descriptor set
    if (program.data_dirty) {
        auto &descriptor_set = find_or_create_descriptor_sets(api, program);

        std::vector<vk::WriteDescriptorSet> writes;
        map_transform(program.binded_data, writes, [&](const auto &binded_data) {
            vk::WriteDescriptorSet write;
            write.dstSet           = descriptor_set.set;
            write.dstBinding       = binded_data->binding;
            write.descriptorCount  = 1;
            write.descriptorType   = binded_data->type;
            write.pImageInfo       = &binded_data->image_info;
            write.pBufferInfo      = &binded_data->buffer_info;
            write.pTexelBufferView = &binded_data->buffer_view;
            return write;
        });

        api.ctx.device->updateDescriptorSets(writes, nullptr);

        program.data_dirty = false;
    }

    const auto &descriptor_set = program.descriptor_sets[program.current_descriptor_set].set;
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *program.pipeline_layout, 0, descriptor_set, {});
}

void API::draw_indexed(u32 index_count, u32 instance_count, u32 first_index, i32 vertex_offset, u32 first_instance)
{
    pre_draw(*this, *current_program);
    auto &frame_resource = ctx.frame_resources.get_current();
    frame_resource.command_buffer->drawIndexed(index_count, instance_count, first_index, vertex_offset, first_instance);
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

} // namespace my_app::vulkan
