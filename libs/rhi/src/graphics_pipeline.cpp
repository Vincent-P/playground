#include "rhi/pipelines.h"

#include "exo/collections/array.h"

#include "rhi/device.h"
#include "rhi/utils.h"

namespace rhi
{
Handle<GraphicsProgram> Device::create_program(exo::StringView name, const GraphicsState &graphics_state)
{
	auto attachments_count = graphics_state.attachments_format.attachments_format.len() +
	                         (graphics_state.attachments_format.depth_format.has_value() ? 1 : 0);

	auto   load_ops   = Vec<LoadOp>::with_values(attachments_count, LoadOp::ignore());
	auto renderpass = create_renderpass(*this, graphics_state.attachments_format, load_ops);

	return graphics_programs.add({
		.name           = exo::String{name},
		.graphics_state = graphics_state,
		.renderpass     = renderpass.vkhandle,
	});
}

void Device::destroy_program(Handle<GraphicsProgram> program_handle)
{
	auto &program = graphics_programs.get(program_handle);
	for (auto pipeline : program.pipelines) {
		vkDestroyPipeline(device, pipeline, nullptr);
	}

	vkDestroyRenderPass(device, program.renderpass, nullptr);

	graphics_programs.remove(program_handle);
}

u32 Device::compile_graphics_state(Handle<GraphicsProgram> &program_handle, const RenderState &render_state)
{
	auto &program = graphics_programs.get(program_handle);

	program.render_states.push(render_state);

	const u32 i_pipeline = static_cast<u32>(program.pipelines.len());
	program.pipelines.push(VK_NULL_HANDLE);
	compile_graphics_pipeline(program_handle, i_pipeline);
	return i_pipeline;
}

void Device::compile_graphics_pipeline(Handle<GraphicsProgram> &program_handle, usize i_pipeline)
{
	auto &program = graphics_programs.get(program_handle);

	const auto &render_state = program.render_states[i_pipeline];

	VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

	VkPipelineDynamicStateCreateInfo dyn_i = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
	dyn_i.dynamicStateCount                = static_cast<u32>(exo::Array::len(dynamic_states));
	dyn_i.pDynamicStates                   = dynamic_states;

	const VkPipelineVertexInputStateCreateInfo vert_i = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

	const VkPipelineInputAssemblyStateCreateInfo asm_i = {
		.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.flags                  = 0,
		.topology               = to_vk(render_state.input_assembly.topology),
		.primitiveRestartEnable = VK_FALSE,
	};

	VkPipelineRasterizationConservativeStateCreateInfoEXT conservative = {
		.sType                            = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT,
		.conservativeRasterizationMode    = VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT,
		.extraPrimitiveOverestimationSize = 0.1f, // in pixels
	};

	const VkPipelineRasterizationStateCreateInfo rast_i = {
		.sType            = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.pNext            = render_state.rasterization.enable_conservative_rasterization ? &conservative : nullptr,
		.flags            = 0,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode             = VK_POLYGON_MODE_FILL,
		.cullMode  = VkCullModeFlags(render_state.rasterization.culling ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE),
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.depthBiasEnable         = render_state.depth.bias != 0.0f,
		.depthBiasConstantFactor = render_state.depth.bias,
		.depthBiasClamp          = 0,
		.depthBiasSlopeFactor    = 0,
		.lineWidth               = 1.0f,
	};

	Vec<VkPipelineColorBlendAttachmentState> att_states;
	att_states.reserve(program.graphics_state.attachments_format.attachments_format.len());

	for (usize i_color = 0; i_color < program.graphics_state.attachments_format.attachments_format.len(); ++i_color) {
		att_states.push();
		auto &state = att_states.last();
		state.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		state.blendEnable = VK_FALSE;

		if (render_state.alpha_blending) {
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

	VkPipelineColorBlendStateCreateInfo colorblend_i = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
	colorblend_i.flags             = 0;
	colorblend_i.attachmentCount   = static_cast<u32>(att_states.len());
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
	ds_i.flags                                 = 0;
	ds_i.depthTestEnable                       = render_state.depth.test ? VK_TRUE : VK_FALSE;
	ds_i.depthWriteEnable                      = render_state.depth.enable_write ? VK_TRUE : VK_FALSE;
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

	exo::DynamicArray<VkPipelineShaderStageCreateInfo, 2> shader_stages;

	if (program.graphics_state.vertex_shader.is_valid()) {
		const auto                     &shader      = shaders.get(program.graphics_state.vertex_shader);
		VkPipelineShaderStageCreateInfo create_info = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
		create_info.stage                           = VK_SHADER_STAGE_VERTEX_BIT;
		create_info.module                          = shader.vkhandle;
		create_info.pName                           = "main";
		shader_stages.push(create_info);
	}

	if (program.graphics_state.fragment_shader.is_valid()) {
		const auto                     &shader      = shaders.get(program.graphics_state.fragment_shader);
		VkPipelineShaderStageCreateInfo create_info = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
		create_info.stage                           = VK_SHADER_STAGE_FRAGMENT_BIT;
		create_info.module                          = shader.vkhandle;
		create_info.pName                           = "main";
		shader_stages.push(create_info);
	}

	VkGraphicsPipelineCreateInfo pipe_i = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
	pipe_i.layout                       = global_sets.pipeline_layout;
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
	pipe_i.stageCount                   = static_cast<u32>(shader_stages.len());
	pipe_i.renderPass                   = program.renderpass;
	pipe_i.subpass                      = 0;

	vk_check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipe_i, nullptr, &program.pipelines[i_pipeline]));

	if (vkSetDebugUtilsObjectNameEXT) {
		VkDebugUtilsObjectNameInfoEXT ni = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
		ni.objectHandle                  = reinterpret_cast<u64>(program.pipelines[i_pipeline]);
		ni.objectType                    = VK_OBJECT_TYPE_PIPELINE;
		ni.pObjectName                   = program.name.c_str();
		vk_check(vkSetDebugUtilsObjectNameEXT(device, &ni));
	}
}
} // namespace rhi
