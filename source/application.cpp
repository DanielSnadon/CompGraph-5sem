#include "application.hpp"

#include <imgui.h>
#include <iostream>

namespace application {

VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

VkShaderModule vertexShader = VK_NULL_HANDLE;
VkShaderModule fragmentShader = VK_NULL_HANDLE;

bool initialize() {

	// 1. Pipeline layout creation

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	if (vkCreatePipelineLayout(
			graphics::internal::context.device,
			&layoutInfo,
			nullptr,
			&pipelineLayout) != VK_SUCCESS)
		{
			std::cerr << "Не удалость создать pipeline layout.\n";
			return false;
		}

	// 2. Using shaders...

	VkPipelineShaderStageCreateInfo shaderStages[2]{};

	shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStages[0].module = vertexShader;
	shaderStages[0].pName = "main";

	shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStages[1].module = fragmentShader;
	shaderStages[1].pName = "main";

	// 3. Vector Input / Input Assembly

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
	inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	// 4. Scissor / Viewport

	VkPipelineViewportStateCreateInfo viewportInfo{};
	viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportInfo.viewportCount = 1;
	viewportInfo.scissorCount = 1;

	// 5. Rasterization

	VkPipelineRasterizationStateCreateInfo rasterInfo{};
	rasterInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterInfo.cullMode = VK_CULL_MODE_NONE;
	rasterInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterInfo.lineWidth = 1.0f;

	// 6. Depth test

	VkPipelineDepthStencilStateCreateInfo depthInfo{};
	depthInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthInfo.depthTestEnable = VK_TRUE;
	depthInfo.depthWriteEnable = VK_TRUE;
	depthInfo.depthCompareOp = VK_COMPARE_OP_LESS;

	// 7. Multisampling

	VkPipelineMultisampleStateCreateInfo sampleInfo{};
	sampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	sampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// 8. Color blending

	VkPipelineColorBlendAttachmentState colorAttachment{};
	colorAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
										VK_COLOR_COMPONENT_G_BIT |
										VK_COLOR_COMPONENT_B_BIT |
										VK_COLOR_COMPONENT_A_BIT;
	colorAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
	colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendInfo.attachmentCount = 1;
	colorBlendInfo.pAttachments = &colorAttachment;

	return true;
}

void shutdown() {
	auto& context = graphics::internal::context;
	vkQueueWaitIdle(context.graphics_queue);

	vkDestroyPipelineLayout(context.device, pipelineLayout, nullptr);
}

void update([[maybe_unused]] double time) {
	ImGui::ShowDemoWindow();
}

void render(const graphics::internal::FrameData& fd) {
	
	// I. Clearance and fill with dark color.

	auto& context = graphics::internal::context;

	vkResetCommandBuffer(fd.command_buffer, 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	vkBeginCommandBuffer(fd.command_buffer, &beginInfo);

	VkClearValue clearValues[2]{};
	clearValues[0].color = {{0.01f, 0.01f, 0.01f, 1.0f}};
	clearValues[1].depthStencil = {1.0f, 0};

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = context.render_pass;
	renderPassInfo.framebuffer = fd.framebuffer;
	renderPassInfo.renderArea.extent = context.swapchain_extent;
	renderPassInfo.clearValueCount = 2;
	renderPassInfo.pClearValues = clearValues;

	vkCmdBeginRenderPass(fd.command_buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdEndRenderPass(fd.command_buffer);
	vkEndCommandBuffer(fd.command_buffer);
}
}