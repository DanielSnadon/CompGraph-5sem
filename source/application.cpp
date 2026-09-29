#include "application.hpp"

#include <fstream>
#include <vector>
#include <imgui.h>
#include <iostream>
#include <cmath>
#include <numbers>
#include <string>
#include <cstring>
#include <cstddef>
#include <array>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace application {

glm::vec3 objectColor{1.0f, 1.0f, 1.0f};

bool animation = false;
float animationSpeed = 1.0f;
float trajectoryRadius = 0.7f;
float trajectoryHeight = 0.3f;
float animationPhase = 0.0f;

glm::vec3 objectPosition{0.0f, 0.0f, 0.0f};
glm::vec3 objectRotationDegrees{0.0f, 0.0f, 0.0f};
glm::vec3 objectScale{1.0f, 1.0f, 1.0f};
int perspectiveProjection = 1;

struct Vertex {
	float position[3];
	float color[3];
};

struct UniformBufferObject {
	alignas(16) glm::mat4 model{1.0f};
	alignas(16) glm::mat4 view{1.0f};
	alignas(16) glm::mat4 projection{1.0f};
	alignas(16) glm::vec3 color{1.0f, 1.0f, 1.0f};
};
// P.S:
// Координаты модели ------> Мировые координаты ------> Координаты относительно камеры ------> Координаты экрана
//                   modelMatrix              viewMatrix                        projectionMatrix

struct RenderObject {
	glm::vec3 position{0.0f};
	glm::vec3 rotationDegrees{0.0f};
	glm::vec3 scale{1.0f};

	VkBuffer uniformBuffer = VK_NULL_HANDLE;
	VmaAllocation uniformBufferAllocation = nullptr;
	UniformBufferObject* uniformBufferMemory = nullptr;
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
};

std::array<RenderObject, 3> renderObjects{};

std::vector<Vertex> cylinderVertices;
std::vector<uint32_t> cylinderIndices;

constexpr uint32_t cylinderSegments = 50;

VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

VkShaderModule vertexShader = VK_NULL_HANDLE;
VkShaderModule fragmentShader = VK_NULL_HANDLE;

VkPipeline graphicsPipeline = VK_NULL_HANDLE;

VkBuffer vertexBuffer = VK_NULL_HANDLE;
VmaAllocation vertexBufferAllocation = nullptr;
Vertex* vertexBufferMemory = nullptr;

VkBuffer indexBuffer = VK_NULL_HANDLE;
VmaAllocation indexBufferAllocation = nullptr;
uint32_t* indexBufferMemory = nullptr;

VkBuffer uniformBuffer = VK_NULL_HANDLE;
VmaAllocation uniformBufferAllocation = nullptr;
UniformBufferObject* uniformBufferMemory = nullptr;

VkShaderModule loadShaderModule(const char path[]) {
	std::ifstream file(path, std::ios::binary | std::ios::ate);

	const size_t size = file.tellg();

	std::vector<uint32_t> buffer(size / sizeof(uint32_t));

	file.seekg(0);
	file.read(reinterpret_cast<char*>(buffer.data()), size);
	file.close();

	VkShaderModuleCreateInfo info{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = size,
		.pCode = buffer.data(),
	};
	
	VkShaderModule result;
	if (vkCreateShaderModule(graphics::internal::context.device, &info, nullptr, &result) != VK_SUCCESS)
	{
		return nullptr;
	}
	return result;
}

// Building цилиндр

void generateCylinderGeometry() {
	cylinderVertices.clear();
	cylinderIndices.clear();

	cylinderVertices.reserve(cylinderSegments * 2 + 2);
	cylinderIndices.reserve(cylinderSegments * 12);

	constexpr float radius = 0.5f;
	constexpr float height = 0.7f;

	for (uint32_t i = 0; i < cylinderSegments; ++i)
	{
		const float angle = 2.0f * std::numbers::pi_v<float> * float(i) / float(cylinderSegments);

		const float red = 0.5f + 0.5f * std::sin(angle);
		const float green = 0.5f + 0.5f * std::sin(angle + 2.0f * std::numbers::pi_v<float> / 3.0f);
		const float blue = 0.5f + 0.5f * std::sin(angle + 4.0f * std::numbers::pi_v<float> / 3.0f);
		
		const float x = radius * std::cos(angle);
		const float z = radius * std::sin(angle);

		cylinderVertices.push_back({
			.position = {x, -height, z},
			.color = {red, green, blue},
		});

		cylinderVertices.push_back({
			.position = {x, height, z},
			.color = {red, green, blue},
		});
	}

	cylinderVertices.push_back({
		.position = {0.0f, -height, 0.0f},
		.color = {0.5f, 0.5f, 0.5f},
	});

	cylinderVertices.push_back({
		.position = {0.0f, height, 0.0f},
		.color = {0.5f, 0.5f, 0.5f},
	});

	const uint32_t bottomCenter = cylinderSegments * 2;
	const uint32_t topCenter = bottomCenter + 1;

	for (uint32_t i = 0; i < cylinderSegments; ++i)
	{
		const uint32_t next = (i + 1) % cylinderSegments;

		const uint32_t bottom = i * 2;
		const uint32_t top = bottom + 1;

		const uint32_t nextBottom = next * 2;
		const uint32_t nextTop = nextBottom + 1;

		cylinderIndices.insert(cylinderIndices.end(), {
			bottom, top, nextBottom,
			nextBottom, top, nextTop,
			bottomCenter, bottom, nextBottom,
			topCenter, nextTop, top,
		});
	}

}

bool initialize() {

	// Cylinder(s?)

	generateCylinderGeometry();

	renderObjects[0].position = {-0.8f, 0.0f, 0.0f};
	renderObjects[1].position = {0.0f, 0.0f, 0.0f};
	renderObjects[2].position = {0.8f, 0.0f, 0.0f};

	for (RenderObject& object : renderObjects)
	{
		object.scale = glm::vec3(0.65f);
	}

	// Vertex Buffer

	auto& context = graphics::internal::context;

	const size_t vertexDataSize = cylinderVertices.size() * sizeof(Vertex);

	VkBufferCreateInfo vertexBufferInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = vertexDataSize,
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VmaAllocationCreateInfo vertexAllocationInfo{
		.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	if (vmaCreateBuffer(context.allocator, &vertexBufferInfo, &vertexAllocationInfo, &vertexBuffer, &vertexBufferAllocation, nullptr) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать vertex buffer.\n";
		return false;
	}

	if (vmaMapMemory(context.allocator, vertexBufferAllocation, reinterpret_cast<void**>(&vertexBufferMemory)) != VK_SUCCESS)
	{
		std::cerr << "Не удалось отобразить vertex buffer память.\n";
		return false;
	}

	memcpy(vertexBufferMemory, cylinderVertices.data(), vertexDataSize);

	vmaUnmapMemory(context.allocator, vertexBufferAllocation);
	vertexBufferMemory = nullptr;

	// Index Buffer

	const size_t indexDataSize = cylinderIndices.size() * sizeof(uint32_t);

	VkBufferCreateInfo indexBufferInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = indexDataSize,
		.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VmaAllocationCreateInfo indexAllocationInfo{
		.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	if (vmaCreateBuffer(context.allocator, &indexBufferInfo, &indexAllocationInfo, &indexBuffer, &indexBufferAllocation, nullptr) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать index buffer.\n";
		return false;
	}

	if (vmaMapMemory(context.allocator, indexBufferAllocation, reinterpret_cast<void**>(&indexBufferMemory)) != VK_SUCCESS)
	{
		std::cerr << "Не удалось отобразить index buffer память.\n";
		return false;
	}

	memcpy(indexBufferMemory, cylinderIndices.data(), indexDataSize);

	vmaUnmapMemory(context.allocator, indexBufferAllocation);
	indexBufferMemory = nullptr;
	
	// Uniform Buffer

	VkBufferCreateInfo uniformBufferInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(UniformBufferObject),
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VmaAllocationCreateInfo uniformAllocationInfo{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	if (vmaMapMemory(context.allocator, uniformBufferAllocation, reinterpret_cast<void**>(&uniformBufferMemory)) != VK_SUCCESS)
	{
		std::cerr << "Не удалось отобразить uniform buffer память.\n";
		return false;
	}

	// Dos
	for (RenderObject& object : renderObjects)
	{
		if (vmaCreateBuffer(context.allocator, &uniformBufferInfo, &uniformAllocationInfo, &object.uniformBuffer, &object.uniformBufferAllocation, nullptr) != VK_SUCCESS)
		{
			std::cerr << "Не удалось создать uniform buffer.\n";
			return false;
		}
		if (vmaMapMemory(context.allocator, object.uniformBufferAllocation, reinterpret_cast<void**>(&object.uniformBufferMemory)) != VK_SUCCESS)
		{
			std::cerr << "Не удалось отобразить uniform buffer память.\n";
			return false;
		}
	}

	// Matrix calculations
	// *uniformBufferMemory = UniformBufferObject{}; <- starting form

	uniformBufferMemory->model = glm::rotate( // Cylinder
		glm::mat4(1.0f),
		glm::radians(25.0f),
		glm::vec3(1.0f, 0.0f, 0.0f)
	);

	uniformBufferMemory->view = glm::lookAt( // Camera
		glm::vec3(2.0f, 1.8f, 2.5f),
		glm::vec3(0.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f)
	);

	uniformBufferMemory->projection = glm::perspective( // Projection
		glm::radians(45.0f),
		static_cast<float>(context.swapchain_extent.width) / static_cast<float>(context.swapchain_extent.height), 0.1f, 10.0f
	);

	uniformBufferMemory->projection[1][1] *= -1.0f;

	vmaFlushAllocation(context.allocator, uniformBufferAllocation, 0, sizeof(UniformBufferObject));

	// Descriptor layout

	VkDescriptorSetLayoutBinding uniformLayoutBinding{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.pImmutableSamplers = nullptr,
	};

	VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 1,
		.pBindings = &uniformLayoutBinding,
	};

	if (vkCreateDescriptorSetLayout(context.device, &descriptorLayoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
	{
		std::cerr << "Не удалось созать descriptor set layout.\n";
		return false;
	}

	// Descriptor pool

	VkDescriptorPoolSize VkDescriptorPoolSize{
		.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 4,
	};
	
	VkDescriptorPoolCreateInfo descriptorPoolInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = 4,
		.poolSizeCount = 1,
		.pPoolSizes = &VkDescriptorPoolSize,
	};

	if (vkCreateDescriptorPool(context.device, &descriptorPoolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать descriptor pool.\n";
		return false;
	}

	// Descriptor set

	// Dos
	for (RenderObject& object : renderObjects)
	{
		VkDescriptorSetAllocateInfo objectSetAllocateInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = descriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts = &descriptorSetLayout,
		};

		if (vkAllocateDescriptorSets(context.device, &objectSetAllocateInfo, &object.descriptorSet) != VK_SUCCESS)
		{
			std::cerr << "Не удалось выделить descriptor set.\n";
			return false; 
		}

		VkDescriptorBufferInfo objectBufferInfo{
			.buffer = object.uniformBuffer,
			.offset = 0,
			.range = sizeof(UniformBufferObject),
		};

		VkWriteDescriptorSet objectDescriptorWrite{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = object.descriptorSet,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &objectBufferInfo,
		};

		vkUpdateDescriptorSets(context.device, 1, &objectDescriptorWrite, 0, nullptr);
	}

	// Pipeline layout

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &descriptorSetLayout;

	if (vkCreatePipelineLayout(graphics::internal::context.device, &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
	{
		std::cerr << "Не удалость создать pipeline layout.\n";
		return false;
	}
	
	// Using shaders...

	vertexShader = loadShaderModule("shaders/basic.vert.spv");
	fragmentShader = loadShaderModule("shaders/basic.frag.spv");

	if (vertexShader == VK_NULL_HANDLE || fragmentShader == VK_NULL_HANDLE)
	{
		std::cerr << "Не удалось загрузить шейдеры.\n";
		return false;
	}
	
	VkPipelineShaderStageCreateInfo shaderStages[2]{};

	shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStages[0].module = vertexShader;
	shaderStages[0].pName = "main";

	shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStages[1].module = fragmentShader;
	shaderStages[1].pName = "main";

	// Vector Input / Input Assembly

	VkVertexInputBindingDescription vertexBinding{
		.binding = 0,
		.stride = sizeof(Vertex),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};

	VkVertexInputAttributeDescription vertexAttributes[2]{
		{
			.location = 0,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(Vertex, position),
		},
		{
			.location = 1,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(Vertex, color),
		},
	};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &vertexBinding;

	vertexInputInfo.vertexAttributeDescriptionCount = 2;
	vertexInputInfo.pVertexAttributeDescriptions = vertexAttributes;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
	inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	// Scissor / Viewport

	VkPipelineViewportStateCreateInfo viewportInfo{};
	viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportInfo.viewportCount = 1;
	viewportInfo.scissorCount = 1;

	// Rasterization

	VkPipelineRasterizationStateCreateInfo rasterInfo{};
	rasterInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterInfo.cullMode = VK_CULL_MODE_NONE;
	rasterInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterInfo.lineWidth = 1.0f;

	// Depth test

	VkPipelineDepthStencilStateCreateInfo depthInfo{};
	depthInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthInfo.depthTestEnable = VK_TRUE;
	depthInfo.depthWriteEnable = VK_TRUE;
	depthInfo.depthCompareOp = VK_COMPARE_OP_LESS;

	// Multisampling

	VkPipelineMultisampleStateCreateInfo sampleInfo{};
	sampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	sampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// Color blending

	VkPipelineColorBlendAttachmentState colorAttachment{};
	colorAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
										VK_COLOR_COMPONENT_G_BIT |
										VK_COLOR_COMPONENT_B_BIT |
										VK_COLOR_COMPONENT_A_BIT;
	colorAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo blendInfo{};
	blendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blendInfo.attachmentCount = 1;
	blendInfo.pAttachments = &colorAttachment;

	// Scissor and viewport - part 2: dynamic states

	const VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicInfo{};
	dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicInfo.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
	dynamicInfo.pDynamicStates = dynamicStates;

	// Pipeline info

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;

	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
	pipelineInfo.pViewportState = &viewportInfo;
	pipelineInfo.pRasterizationState = &rasterInfo;
	pipelineInfo.pDepthStencilState = &depthInfo;
	pipelineInfo.pMultisampleState = &sampleInfo;
	pipelineInfo.pColorBlendState = &blendInfo;
	pipelineInfo.pDynamicState = &dynamicInfo;

	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = graphics::internal::context.render_pass;
	pipelineInfo.subpass = 0;

	// The pipeline

	if (vkCreateGraphicsPipelines(graphics::internal::context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать графический pipeline.\n";
		return false;
	}

	return true;
}

void shutdown() {
	auto& context = graphics::internal::context;
	vkQueueWaitIdle(context.graphics_queue);

	vkDestroyDescriptorPool(context.device, descriptorPool, nullptr);
	descriptorSet = VK_NULL_HANDLE;

	// Dos
	for (RenderObject& object : renderObjects)
	{
		vmaUnmapMemory(context.allocator, object.uniformBufferAllocation);
		
		vmaDestroyBuffer(context.allocator, object.uniformBuffer, object.uniformBufferAllocation);
	}

	vmaDestroyBuffer(context.allocator, indexBuffer, indexBufferAllocation);
	vmaDestroyBuffer(context.allocator, vertexBuffer, vertexBufferAllocation);

	vkDestroyPipeline(context.device, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(context.device, pipelineLayout, nullptr);

	vkDestroyDescriptorSetLayout(context.device, descriptorSetLayout, nullptr);

	vkDestroyShaderModule(context.device, vertexShader, nullptr);
	vkDestroyShaderModule(context.device, fragmentShader, nullptr);
}

void update([[maybe_unused]] double time) {

	// 1) Time

	static double prevTime = time;
	const double delta = time - prevTime;
	prevTime = time;

	// 2) ImGui stuff

	ImGui::Begin("Settings");
	ImGui::RadioButton("Perspective", &perspectiveProjection, 1);
	ImGui::SameLine();
	ImGui::RadioButton("Orthographic", &perspectiveProjection, 0);

	ImGui::SliderFloat3("Position", glm::value_ptr(objectPosition), -2.0f, 2.0f);
	ImGui::SliderFloat3("Rotation", glm::value_ptr(objectRotationDegrees), -180.0f, 180.0f);
	ImGui::SliderFloat3("Scale", glm::value_ptr(objectScale), 0.1f, 2.0f);

	ImGui::Checkbox("Animation", &animation);
	ImGui::SliderFloat("Animation speed", &animationSpeed, -3.0f, 3.0f);
	ImGui::SliderFloat("Trajectory radius", &trajectoryRadius, 0.0f, 1.5f);
	ImGui::SliderFloat("Trajectory height", &trajectoryHeight, 0.0f, 1.0f);

	ImGui::ColorEdit3("Color", glm::value_ptr(objectColor));

	ImGui::End();

	// 3) Animation

	if (animation)
	{
		animationPhase += static_cast<float>(delta) * animationSpeed;

		animationPhase = std::fmod(animationPhase, 2.0f * std::numbers::pi_v<float>);
	}
}

void render(const graphics::internal::FrameData& fd) {

	auto& context = graphics::internal::context;

	// I. Animation preparation

	const float phase = animationPhase;

	const glm::vec3 animatedPos = objectPosition + 
		glm::vec3(
			trajectoryRadius * std::cos(2.0f * phase),
			trajectoryHeight * std::sin(phase),
			trajectoryRadius * std::sin(2.0f * phase)
		);
	
	glm::vec3 animatedRot = objectRotationDegrees;
	animatedRot.x += 180.0f * std::cos(phase);
	animatedRot.y += 180.0f * std::cos(phase);
	animatedRot.z += 180.0f * std::sin(phase);

	// II. View

	const float aspect = static_cast<float>(context.swapchain_extent.width) / static_cast<float>(context.swapchain_extent.height);
	
	const glm::mat4 view = glm::lookAt(
		glm::vec3(2.0f, 1.8f, 2.5f),
		glm::vec3(0.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f)
	);

	glm::mat4 projection;

	if (perspectiveProjection)
	{
		projection = glm::perspective(
			glm::radians(45.0f),
			aspect,
			0.1f,
			10.0f
		);
	}
	else {
		const float halfHeight = 0.9f;
		const float halfWidth = halfHeight * aspect;

		projection = glm::ortho(
			-halfWidth,
			halfWidth,
			-halfHeight,
			halfHeight,
			0.1f,
			10.0f
		);
	}

	projection[1][1] *= -1.0f;

	for (RenderObject& object : renderObjects)
	{
		glm::mat4 model(1.0f);

		model = glm::translate(model, animatedPos + objectPosition);

		const glm::vec3 rotation = animatedRot + object.rotationDegrees;

		model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		model = glm::scale(model, objectScale * object.scale);

		object.uniformBufferMemory->model = model;
		object.uniformBufferMemory->view = view;
		object.uniformBufferMemory->projection = projection;
		object.uniformBufferMemory->color = objectColor;

		vmaFlushAllocation(context.allocator, object.uniformBufferAllocation, 0, sizeof(UniformBufferObject));
	}
	// III. Clearance and fill with dark color.

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

	// IV. Main.

	vkCmdBeginRenderPass(fd.command_buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	const VkViewport viewport{
		.x = 0, .y = 0,
		.width = float(context.swapchain_extent.width),
		.height = float(context.swapchain_extent.height),
		.minDepth = 0.0f, .maxDepth = 1.0
	};

	const VkRect2D scissor{
		.extent = context.swapchain_extent,
	};

	vkCmdSetViewport(fd.command_buffer, 0, 1, &viewport);
	vkCmdSetScissor(fd.command_buffer, 0, 1, &scissor);

	vkCmdBindPipeline(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

	VkDeviceSize vertexBufferOffset = 0;

	vkCmdBindVertexBuffers(fd.command_buffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

	vkCmdBindIndexBuffer(fd.command_buffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

	for (const RenderObject& object : renderObjects)
	{
		vkCmdBindDescriptorSets(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &object.descriptorSet, 0, nullptr);
		vkCmdDrawIndexed(fd.command_buffer, static_cast<uint32_t>(cylinderIndices.size()), 1, 0, 0, 0);
	}

	vkCmdEndRenderPass(fd.command_buffer);
	vkEndCommandBuffer(fd.command_buffer);
}
}
