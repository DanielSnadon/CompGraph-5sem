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
float animationSpeed = 0.5f;
float trajectoryRadius = 0.0f;
float trajectoryHeight = 0.0f;
float animationPhase = 0.0f;

glm::vec3 objectPosition{0.0f, 0.0f, 0.0f};
glm::vec3 objectRotationDegrees{0.0f, 0.0f, 0.0f};
glm::vec3 objectScale{1.0f, 1.0f, 1.0f};
int perspectiveProjection = 1;

// [new] camera const
float cameraFOV = 45.0f;
float cameraYaw = -130.0f;
float cameraPitch = -30.0f;
glm::vec3 cameraPosition(2.0f, 1.8f, 2.5f);

// [new] light const
float shininess = 32.0f;
float ambient = 0.15f;
glm::vec3 ambientColor(1.0f);
glm::vec3 specularColor(0.8f);
glm::vec3 lightDirection(-0.5f, 1.0f, 0.3f);
glm::vec3 lightColor(1.0f);
float lightIntensity = 1.0f;

struct Vertex {
	float position[3];
	float color[3];
	float normal[3];
};

// [new] markers

const std::array<Vertex, 8> lightMarkerVertices{{
    {{-0.5f, -0.5f, -0.5f}, {1, 1, 1}, {0, 1, 0}},
    {{ 0.5f, -0.5f, -0.5f}, {1, 1, 1}, {0, 1, 0}},
    {{ 0.5f,  0.5f, -0.5f}, {1, 1, 1}, {0, 1, 0}},
    {{-0.5f,  0.5f, -0.5f}, {1, 1, 1}, {0, 1, 0}},
    {{-0.5f, -0.5f,  0.5f}, {1, 1, 1}, {0, 1, 0}},
    {{ 0.5f, -0.5f,  0.5f}, {1, 1, 1}, {0, 1, 0}},
    {{ 0.5f,  0.5f,  0.5f}, {1, 1, 1}, {0, 1, 0}},
    {{-0.5f,  0.5f,  0.5f}, {1, 1, 1}, {0, 1, 0}},
}};
const std::array<uint32_t, 36> lightMarkerIndices{
    0, 2, 1,  0, 3, 2,  // -Z
    4, 5, 6,  4, 6, 7,  // +Z
    0, 4, 7,  0, 7, 3,  // -X
    1, 2, 6,  1, 6, 5,  // +X
    0, 1, 5,  0, 5, 4,  // -Y
    3, 7, 6,  3, 6, 2,  // +Y
};

// unused.
// struct UniformBufferObject {
// 	alignas(16) glm::mat4 model{1.0f};
// 	alignas(16) glm::mat4 view{1.0f};
// 	alignas(16) glm::mat4 projection{1.0f};
// 	alignas(16) glm::vec3 color{1.0f, 1.0f, 1.0f};
// 	alignas(16) glm::vec4 cameraPosition;
// 	alignas(16) glm::vec4 materialParameters;
// 	alignas(16) glm::vec4 specularColor;
// 	alignas(16) glm::vec4 lightDirection;
// 	alignas(16) glm::vec4 lightColorIntensity;
// };

// [new] struct

struct SceneUniformBufferObject {
    alignas(16) glm::mat4 viewProjection{1.0f};
    alignas(16) glm::vec4 cameraPosition{0.0f};
    alignas(16) glm::vec4 ambientColorIntensity{1.0f, 1.0f, 1.0f, 0.15f};
	alignas(16) glm::vec4 lightDirection{-0.5f, 1.0f, 0.3f, 0.0f};
	alignas(16) glm::vec4 lightColorIntensity{1.0f};
	alignas(16) glm::ivec4 lightCounts{0};
};

struct ModelUniformBufferObject {
    alignas(16) glm::mat4 model{1.0f};
    alignas(16) glm::vec4 color{1.0f};
    alignas(16) glm::vec4 materialParameters{32.0f, 0.0f, 0.0f, 0.0f};
    alignas(16) glm::vec4 specularColor{0.8f, 0.8f, 0.8f, 1.0f};
};

struct PointLight {
	alignas(16) glm::vec4 position{0.0f, 2.0f, 0.0f, 1.0f};
	alignas(16) glm::vec4 colorIntensity{1.0f, 1.0f, 1.0f, 1.0f};
};

struct SpotLight {
    alignas(16) glm::vec4 position{0.0f, 2.0f, 0.0f, 1.0f};
    alignas(16) glm::vec4 direction{0.0f, -1.0f, 0.0f, 0.0f};
    alignas(16) glm::vec4 colorIntensity{1.0f};
    alignas(16) glm::vec4 coneAnglesDegrees{15.0f, 25.0f, 0.0f, 0.0f};
};

struct RenderObject {
	glm::vec3 position{0.0f};
	glm::vec3 rotationDegrees{0.0f};
	glm::vec3 scale{1.0f};

	VkBuffer uniformBuffer = VK_NULL_HANDLE;
	VmaAllocation uniformBufferAllocation = nullptr;
	ModelUniformBufferObject* uniformBufferMemory = nullptr;
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
};

constexpr uint32_t maxPointLights = 8;

std::array<PointLight, maxPointLights> pointLights{};
int pointLightCount = 1;

constexpr uint32_t maxSpotLights = 8;

std::array<SpotLight, maxSpotLights> spotLights{};
int spotLightCount = 1;

constexpr uint32_t maxLightMarkers = maxPointLights + maxSpotLights;
std::array<RenderObject, maxLightMarkers> lightMarkers{};

constexpr uint32_t spotConeSegments = 16;
constexpr uint32_t spotLineVerticesPerLight = 2 + 2 * spotConeSegments + 8;

VkBuffer spotLineVertexBuffer = VK_NULL_HANDLE;
VmaAllocation spotLineVertexAllocation = nullptr;
Vertex* spotLineVertexMemory = nullptr;

constexpr float lightMarkerSize = 0.1f;

bool showVisualization = true;

// Lab2 be like:
// + something const/struct
// + ImGui sliders
// + into buffer
// + into shaders
// + render

std::array<RenderObject, 3> renderObjects{};

std::vector<Vertex> cylinderVertices;
std::vector<uint32_t> cylinderIndices;

constexpr uint32_t cylinderSegments = 50;

VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

VkDescriptorSetLayout sceneDescriptorSetLayout = VK_NULL_HANDLE;
VkDescriptorSetLayout modelDescriptorSetLayout = VK_NULL_HANDLE;

VkDescriptorSet sceneDescriptorSet = VK_NULL_HANDLE;

VkShaderModule vertexShader = VK_NULL_HANDLE;
VkShaderModule fragmentShader = VK_NULL_HANDLE;

VkPipeline graphicsPipeline = VK_NULL_HANDLE;
VkPipeline lightLinePipeline = VK_NULL_HANDLE;

VkBuffer vertexBuffer = VK_NULL_HANDLE;
VmaAllocation vertexBufferAllocation = nullptr;
Vertex* vertexBufferMemory = nullptr;

VkBuffer indexBuffer = VK_NULL_HANDLE;
VmaAllocation indexBufferAllocation = nullptr;
uint32_t* indexBufferMemory = nullptr;

VkBuffer sceneUniformBuffer = VK_NULL_HANDLE;
VmaAllocation sceneUniformBufferAllocation = nullptr;
SceneUniformBufferObject* sceneUniformBufferMemory = nullptr;

VkBuffer pointLightBuffer = VK_NULL_HANDLE;
VmaAllocation pointLightBufferAllocation = nullptr;
PointLight* pointLightBufferMemory = nullptr;

VkBuffer spotLightBuffer = VK_NULL_HANDLE;
VmaAllocation spotLightBufferAllocation = nullptr;
SpotLight* spotLightBufferMemory = nullptr;

VkBuffer lightMarkerVertexBuffer = VK_NULL_HANDLE;
VmaAllocation lightMarkerVertexAllocation = nullptr;
VkBuffer lightMarkerIndexBuffer = VK_NULL_HANDLE;
VmaAllocation lightMarkerIndexAllocation = nullptr;

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
			.normal = {std::cos(angle), 0.0f, std::sin(angle)},
		});

		cylinderVertices.push_back({
			.position = {x, height, z},
			.color = {red, green, blue},
			.normal = {std::cos(angle), 0.0f, std::sin(angle)},
		});
	}

	cylinderVertices.push_back({
		.position = {0.0f, -height, 0.0f},
		.color = {0.5f, 0.5f, 0.5f},
		.normal = {0.0f, -1.0f, 0.0f},
	});

	cylinderVertices.push_back({
		.position = {0.0f, height, 0.0f},
		.color = {0.5f, 0.5f, 0.5f},
		.normal = {0.0f, 1.0f, 0.0f},
	});

	const uint32_t bottomCenter = cylinderSegments * 2;
	const uint32_t topCenter = bottomCenter + 1;

	// [new] новые точки для оснований

	const uint32_t capStart = static_cast<uint32_t>(cylinderVertices.size());

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
			.normal = {0.0f, -1.0f, 0.0f},
		});

		cylinderVertices.push_back({
			.position = {x, height, z},
			.color = {red, green, blue},
			.normal = {0.0f, 1.0f, 0.0f},
		});
	}

	for (uint32_t i = 0; i < cylinderSegments; ++i)
	{
		const uint32_t next = (i + 1) % cylinderSegments;

		const uint32_t bottom = i * 2;
		const uint32_t top = bottom + 1;

		const uint32_t nextBottom = next * 2;
		const uint32_t nextTop = nextBottom + 1;

		const uint32_t bottomCap = capStart + i * 2;
		const uint32_t topCap = bottomCap + 1;

		const uint32_t nextBottomCap = capStart + next * 2;
		const uint32_t nextTopCap = nextBottomCap + 1;


		cylinderIndices.insert(cylinderIndices.end(), {
			bottom, top, nextBottom,
			nextBottom, top, nextTop,

			bottomCenter, bottomCap, nextBottomCap,
			topCenter, nextTopCap, topCap,
		});
	}

}

#pragma region Initze

bool initialize() {

	// Cylinder(s?)

	generateCylinderGeometry();

	renderObjects[0].position = {-1.0f, 0.0f, 0.0f};
	renderObjects[1].position = {0.0f, 0.0f, 0.0f};
	renderObjects[2].position = {1.0f, 0.0f, 0.0f};

	for (RenderObject& object : renderObjects)
	{
		object.scale = glm::vec3(0.55f);
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
	
	// Double Uniform Buffer
	
	VkBufferCreateInfo sceneUniformBufferInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(SceneUniformBufferObject),
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VmaAllocationCreateInfo sceneUniformAllocationInfo{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	if (vmaCreateBuffer(context.allocator, &sceneUniformBufferInfo, &sceneUniformAllocationInfo, &sceneUniformBuffer, &sceneUniformBufferAllocation, nullptr) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать uniform buffer сцены.\n";
		return false;
	}

	if (vmaMapMemory(context.allocator, sceneUniformBufferAllocation, reinterpret_cast<void**>(&sceneUniformBufferMemory)) != VK_SUCCESS)
	{
		std::cerr << "Не удалось отобразить память uniform buffer сцены.\n";
		return false;
	}

	// Uniform Buffer

	VkBufferCreateInfo uniformBufferInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(ModelUniformBufferObject),
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VmaAllocationCreateInfo uniformAllocationInfo{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

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

	// [new] Markers

	for (RenderObject& marker : lightMarkers)
	{
		if (vmaCreateBuffer(context.allocator, &uniformBufferInfo, &uniformAllocationInfo, &marker.uniformBuffer, &marker.uniformBufferAllocation, nullptr) != VK_SUCCESS)
		{
			std::cerr << "Не удалось создать uniform buffer маркера.\n";
			return false;
		}

		if (vmaMapMemory(context.allocator, marker.uniformBufferAllocation, reinterpret_cast<void**>(&marker.uniformBufferMemory)) != VK_SUCCESS)
		{
			std::cerr << "Не удалось отобразить память маркера.\n";
			return false;
		}
	}

	// [new] PointLight

	VkBufferCreateInfo pointLightBufferInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(PointLight) * maxPointLights,
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};
	VmaAllocationCreateInfo pointLightAllocationInfo{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	if (vmaCreateBuffer(context.allocator, &pointLightBufferInfo, &pointLightAllocationInfo, &pointLightBuffer, &pointLightBufferAllocation, nullptr) != VK_SUCCESS)
	{
		std::cerr << "Не удалось созать point light buffer.\n";
		return false;
	}

	if (vmaMapMemory(context.allocator, pointLightBufferAllocation, reinterpret_cast<void**>(&pointLightBufferMemory)) != VK_SUCCESS)
	{
		std::cerr << "Не удалось отобразить память point light buffer.\n";
    	return false;
	}

	// [new] SpotLight

	VkBufferCreateInfo spotLightBufferInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(SpotLight) * maxSpotLights,
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VmaAllocationCreateInfo spotLightAllocationInfo{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	if (vmaCreateBuffer(context.allocator, &spotLightBufferInfo, &spotLightAllocationInfo, &spotLightBuffer, &spotLightBufferAllocation, nullptr) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать spot light buffer.\n";
		return false;
	}

	if (vmaMapMemory(context.allocator, spotLightBufferAllocation, reinterpret_cast<void**>(&spotLightBufferMemory)) != VK_SUCCESS)
	{
		std::cerr << "Не удалось отобразить память spot light buffer.\n";
		return false;
	}

	// [new] LightMarker
		// Vertex

	VmaAllocationCreateInfo markerAllocationInfo{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	const size_t markerVertexDataSize = lightMarkerVertices.size() * sizeof(Vertex);

	VkBufferCreateInfo markerVertexBufferInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = markerVertexDataSize,
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VmaAllocationInfo markerVertexMemoryInfo{};

	if (vmaCreateBuffer(context.allocator, &markerVertexBufferInfo, &markerAllocationInfo, &lightMarkerVertexBuffer, &lightMarkerVertexAllocation, &markerVertexMemoryInfo) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать vertex buffer маркера.\n";
		return false;
	}

	std::memcpy(
		markerVertexMemoryInfo.pMappedData,
		lightMarkerVertices.data(),
		markerVertexDataSize
	);

	vmaFlushAllocation(context.allocator, lightMarkerVertexAllocation, 0, markerVertexDataSize);
		
		// Index
	
	const size_t markerIndexDataSize = lightMarkerIndices.size() * sizeof(uint32_t);

	VkBufferCreateInfo markerIndexBufferInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = markerIndexDataSize,
		.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VmaAllocationInfo markerIndexMemoryInfo{};

	if (vmaCreateBuffer(context.allocator, &markerIndexBufferInfo, &markerAllocationInfo, &lightMarkerIndexBuffer, &lightMarkerIndexAllocation, &markerIndexMemoryInfo) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать index buffer маркера.\n";
		return false;
	}

	std::memcpy(markerIndexMemoryInfo.pMappedData, lightMarkerIndices.data(), markerIndexDataSize);

	vmaFlushAllocation(context.allocator, lightMarkerIndexAllocation, 0, markerIndexDataSize);

		// SpotLines

	VkBufferCreateInfo spotLineBufferInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(Vertex)
			* maxSpotLights
			* spotLineVerticesPerLight,
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VmaAllocationInfo spotLineMemoryInfo{};

	if (vmaCreateBuffer(context.allocator, &spotLineBufferInfo, &markerAllocationInfo, &spotLineVertexBuffer, &spotLineVertexAllocation, &spotLineMemoryInfo) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать vertex buffer линий.\n";
		return false;
	}

	spotLineVertexMemory = static_cast<Vertex*>(spotLineMemoryInfo.pMappedData);

	// Descriptor layout

	VkDescriptorSetLayoutBinding uniformLayoutBinding{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	};

	VkDescriptorSetLayoutBinding pointLightLayoutBinding{
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
	};

	VkDescriptorSetLayoutBinding layoutBindings[]{
		uniformLayoutBinding,
		pointLightLayoutBinding,
	};

	VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 2,
		.pBindings = layoutBindings,
	};

	if (vkCreateDescriptorSetLayout(context.device, &descriptorLayoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
	{
		std::cerr << "Не удалось созать descriptor set layout.\n";
		return false;
	}

	VkDescriptorSetLayoutBinding spotLightLayoutBinding{
		.binding = 2,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
	};

	VkDescriptorSetLayoutBinding sceneBindings[]{
		uniformLayoutBinding,
		pointLightLayoutBinding,
		spotLightLayoutBinding,
	};

	VkDescriptorSetLayoutCreateInfo sceneLayoutInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 3,
		.pBindings = sceneBindings,
	};

	if (vkCreateDescriptorSetLayout(context.device, &sceneLayoutInfo, nullptr, &sceneDescriptorSetLayout) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать descriptor layout сцены.\n";
		return false;
	}

	VkDescriptorSetLayoutCreateInfo modelLayoutInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 1,
		.pBindings = &uniformLayoutBinding,
	};

	if (vkCreateDescriptorSetLayout(context.device, &modelLayoutInfo, nullptr, &modelDescriptorSetLayout) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать descriptor layout модели.\n";
		return false;
	}

	// Descriptor pool

	const uint32_t descriptorSetCapacity = 1 + static_cast<uint32_t>(renderObjects.size() + lightMarkers.size());

	VkDescriptorPoolSize poolSizes[]{
		{
			.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = descriptorSetCapacity,
		},
		{
			.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 4,
		},
	};

	VkDescriptorPoolCreateInfo descriptorPoolInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = descriptorSetCapacity,
		.poolSizeCount = 2,
		.pPoolSizes = poolSizes,
	};

	if (vkCreateDescriptorPool(context.device, &descriptorPoolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать descriptor pool.\n";
		return false;
	}

	// Descriptor set

	for (RenderObject& object : renderObjects)
	{
		VkDescriptorSetAllocateInfo objectSetAllocateInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = descriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts = &modelDescriptorSetLayout,
		};

		if (vkAllocateDescriptorSets(context.device, &objectSetAllocateInfo, &object.descriptorSet) != VK_SUCCESS)
		{
			std::cerr << "Не удалось выделить descriptor set.\n";
			return false; 
		}

		VkDescriptorBufferInfo objectBufferInfo{
			.buffer = object.uniformBuffer,
			.offset = 0,
			.range = sizeof(ModelUniformBufferObject),
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

	for (RenderObject& marker : lightMarkers)
	{
		VkDescriptorSetAllocateInfo markerSetInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = descriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts = &modelDescriptorSetLayout,
		};

		if (vkAllocateDescriptorSets(context.device, &markerSetInfo, &marker.descriptorSet) != VK_SUCCESS)
		{
			std::cerr << "Не удалось выделить descriptor set маркера.\n";
			return false;
		}

		VkDescriptorBufferInfo markerBufferInfo{
			.buffer = marker.uniformBuffer,
			.offset = 0,
			.range = sizeof(ModelUniformBufferObject),
		};

		VkWriteDescriptorSet markerWrite{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = marker.descriptorSet,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &markerBufferInfo,
		};

		vkUpdateDescriptorSets(context.device, 1, &markerWrite, 0, nullptr);
	}

	VkDescriptorSetAllocateInfo sceneSetAllocateInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = descriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &sceneDescriptorSetLayout,
	};

	if (vkAllocateDescriptorSets(context.device, &sceneSetAllocateInfo, &sceneDescriptorSet) != VK_SUCCESS)
	{
		std::cerr << "Не удалось выделить descriptor set сцены.\n";
		return false;
	}

	VkDescriptorBufferInfo sceneUniformDescriptorInfo{
		.buffer = sceneUniformBuffer,
		.offset = 0,
		.range = sizeof(SceneUniformBufferObject),
	};

	VkDescriptorBufferInfo scenePointLightDescriptorInfo{
		.buffer = pointLightBuffer,
		.offset = 0,
		.range = sizeof(PointLight) * maxPointLights,
	};

	VkDescriptorBufferInfo sceneSpotLightDescriptorInfo{
		.buffer = spotLightBuffer,
		.offset = 0,
		.range = sizeof(SpotLight) * maxSpotLights,
	};

	VkWriteDescriptorSet sceneWrites[]{
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = sceneDescriptorSet,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &sceneUniformDescriptorInfo,
		},
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = sceneDescriptorSet,
			.dstBinding = 1,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pBufferInfo = &scenePointLightDescriptorInfo,
		},
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = sceneDescriptorSet,
			.dstBinding = 2,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pBufferInfo = &sceneSpotLightDescriptorInfo,
		},
	};

	vkUpdateDescriptorSets(context.device, 3, sceneWrites, 0, nullptr);

	// Pipeline layout

	const VkDescriptorSetLayout setLayouts[]{
		sceneDescriptorSetLayout,
		modelDescriptorSetLayout,
	};

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 2;
	layoutInfo.pSetLayouts = setLayouts;

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

	VkVertexInputAttributeDescription vertexAttributes[3]{
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
		{
			.location = 2,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(Vertex, normal),
		}
	};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &vertexBinding;

	vertexInputInfo.vertexAttributeDescriptionCount = 3;
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
	pipelineInfo.pDepthStencilState = &depthInfo;
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

	// [new] LIGHT-LINE PIPELINE

	inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

	if (vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &lightLinePipeline) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать pipeline линий света.\n";
		return false;
	}

	return true;
}

#pragma endregion
#pragma region Shdown

void shutdown() {
	auto& context = graphics::internal::context;
	vkQueueWaitIdle(context.graphics_queue);

	vmaDestroyBuffer(context.allocator, spotLineVertexBuffer, spotLineVertexAllocation);

	for (RenderObject& marker : lightMarkers)
	{
		vmaUnmapMemory(context.allocator, marker.uniformBufferAllocation);
		vmaDestroyBuffer(context.allocator, marker.uniformBuffer, marker.uniformBufferAllocation);
	}

	vmaDestroyBuffer(context.allocator, lightMarkerVertexBuffer, lightMarkerVertexAllocation);
	vmaDestroyBuffer(context.allocator, lightMarkerIndexBuffer, lightMarkerIndexAllocation);

	if (spotLightBufferMemory != nullptr)
	{
		vmaUnmapMemory(context.allocator, spotLightBufferAllocation);
		spotLightBufferMemory = nullptr;
	}

	if (spotLightBuffer != VK_NULL_HANDLE)
	{
		vmaDestroyBuffer(context.allocator, spotLightBuffer, spotLightBufferAllocation);
		spotLightBuffer = VK_NULL_HANDLE;
		spotLightBufferAllocation = nullptr;
	}

	if (sceneUniformBufferMemory != nullptr)
	{
		vmaUnmapMemory(context.allocator, sceneUniformBufferAllocation);
		sceneUniformBufferMemory = nullptr;
	}

	if (sceneUniformBuffer != VK_NULL_HANDLE)
	{
		vmaDestroyBuffer(context.allocator, sceneUniformBuffer, sceneUniformBufferAllocation);
		sceneUniformBuffer = VK_NULL_HANDLE;
		sceneUniformBufferAllocation = nullptr;
	}

	if (pointLightBufferMemory != nullptr)
	{
		vmaUnmapMemory(context.allocator, pointLightBufferAllocation);
		pointLightBufferMemory = nullptr;
	}

	if (pointLightBuffer != VK_NULL_HANDLE)
	{
		vmaDestroyBuffer(context.allocator, pointLightBuffer, pointLightBufferAllocation);
		pointLightBuffer = VK_NULL_HANDLE;
		pointLightBufferAllocation = nullptr;
	}

	vkDestroyDescriptorPool(context.device, descriptorPool, nullptr);

	for (RenderObject& object : renderObjects)
	{
		vmaUnmapMemory(context.allocator, object.uniformBufferAllocation);
		
		vmaDestroyBuffer(context.allocator, object.uniformBuffer, object.uniformBufferAllocation);
	}

	vmaDestroyBuffer(context.allocator, indexBuffer, indexBufferAllocation);
	vmaDestroyBuffer(context.allocator, vertexBuffer, vertexBufferAllocation);

	vkDestroyPipeline(context.device, graphicsPipeline, nullptr);
	vkDestroyPipeline(context.device, lightLinePipeline, nullptr);
	vkDestroyPipelineLayout(context.device, pipelineLayout, nullptr);

	vkDestroyDescriptorSetLayout(context.device, sceneDescriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(context.device, modelDescriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(context.device, descriptorSetLayout, nullptr);

	vkDestroyShaderModule(context.device, vertexShader, nullptr);
	vkDestroyShaderModule(context.device, fragmentShader, nullptr);
}

#pragma endregion
#pragma region Update

void update([[maybe_unused]] double time) {

	// 1) Time

	static double prevTime = time;
	const double delta = time - prevTime;
	prevTime = time;

	// 2) ImGui stuff

	ImGui::Begin("Point light settings");

	
	ImGui::SliderInt("Light count", &pointLightCount, 0, static_cast<int>(maxPointLights));

	for (int i = 0; i < pointLightCount; ++i)
	{
		ImGui::PushID(i);

		ImGui::Text("Point light %d", i + 1);

		PointLight& light = pointLights[i];

		ImGui::SliderFloat3("Position", glm::value_ptr(light.position), -5.0f, 5.0f);
		ImGui::ColorEdit3("Color", glm::value_ptr(light.colorIntensity));
		ImGui::SliderFloat("Intensity", &light.colorIntensity.w, 0.0f, 10.0f);

		ImGui::Separator();
		ImGui::PopID();
	}

	ImGui::End();

	ImGui::Begin("Spot light settings");

	ImGui::SliderInt("Light count", &spotLightCount, 0, static_cast<int>(maxSpotLights));

	for (int i = 0; i < spotLightCount; ++i)
	{
		ImGui::PushID(i);

		ImGui::Text("Spot light %d", i + 1);

		SpotLight& light = spotLights[i];

		ImGui::SliderFloat3("Position", glm::value_ptr(light.position), -5.0f, 5.0f);
		ImGui::SliderFloat3("Direction", glm::value_ptr(light.direction), -1.0f, 1.0f);
		ImGui::ColorEdit3("Color", glm::value_ptr(light.colorIntensity));
		ImGui::SliderFloat("Intensity", &light.colorIntensity.w, 0.0f, 10.0f);
		ImGui::SliderFloat("Inner angle", &light.coneAnglesDegrees.x, 1.0f, light.coneAnglesDegrees.y - 1.0f);
		ImGui::SliderFloat("Outer angle", &light.coneAnglesDegrees.y, light.coneAnglesDegrees.x + 1.0f, 89.0f);

		ImGui::Separator();
		ImGui::PopID();
	}

	ImGui::End();

	ImGui::Begin("Lighting settings");

	ImGui::SliderFloat3("Direction", glm::value_ptr(lightDirection), -1.0f, 1.0f);
	ImGui::ColorEdit3("Color", glm::value_ptr(lightColor));
	ImGui::SliderFloat("Intensity", &lightIntensity, 0.0f, 5.0f);

	ImGui::SliderFloat("Specular brightness", &shininess, 1.0f, 128.0f);
	ImGui::ColorEdit3("Specular color", &specularColor.x);

	ImGui::SliderFloat("Ambient strength", &ambient, 0.0f, 1.0f);
	ImGui::ColorEdit3("Ambient color", glm::value_ptr(ambientColor));

	ImGui::Checkbox("Visualization", &showVisualization);

	ImGui::End();

	ImGui::Begin("Camera settings");
	
	ImGui::RadioButton("Perspective", &perspectiveProjection, 1);
	ImGui::SameLine();
	ImGui::RadioButton("Orthographic", &perspectiveProjection, 0);

	ImGui::SliderFloat("Camera FOV", &cameraFOV, 20.0f, 100.0f);

	ImGui::SliderFloat("Camera yaw", &cameraYaw, -180.0f, 180.0f);
	ImGui::SliderFloat("Camera pitch", &cameraPitch, -90.0f, 90.0f);

	ImGui::SliderFloat("Camera X", &cameraPosition.x, -5.0f, 5.0f);
	ImGui::SliderFloat("Camera Y", &cameraPosition.y, -5.0f, 5.0f);
	ImGui::SliderFloat("Camera Z", &cameraPosition.z, -5.0f, 5.0f);

	ImGui::End();

	ImGui::Begin("Animation settings");

	ImGui::Checkbox("Animation", &animation);
	ImGui::SliderFloat("Animation speed", &animationSpeed, -3.0f, 3.0f);
	ImGui::SliderFloat("Trajectory radius", &trajectoryRadius, 0.0f, 1.5f);
	ImGui::SliderFloat("Trajectory height", &trajectoryHeight, 0.0f, 1.0f);

	ImGui::End();

	ImGui::Begin("Cylinder settings");

	ImGui::SliderFloat3("Position", glm::value_ptr(objectPosition), -2.0f, 2.0f);
	ImGui::SliderFloat3("Rotation", glm::value_ptr(objectRotationDegrees), -180.0f, 180.0f);
	ImGui::SliderFloat3("Scale", glm::value_ptr(objectScale), 0.1f, 2.0f);

	ImGui::ColorEdit3("Color", glm::value_ptr(objectColor));

	ImGui::End();

	// 3) Animation

	if (animation)
	{
		animationPhase += static_cast<float>(delta) * animationSpeed;

		animationPhase = std::fmod(animationPhase, 2.0f * std::numbers::pi_v<float>);
	}
}

#pragma endregion

Vertex makeLightLineVertex(const glm::vec3& position)
{
    return Vertex{
        .position = {position.x, position.y, position.z},
        .color = {1.0f, 1.0f, 1.0f},
        .normal = {0.0f, 1.0f, 0.0f},
    };
}

#pragma region Render

void render(const graphics::internal::FrameData& fd) {

	auto& context = graphics::internal::context;

	// [new] I. PointLight && SpotLight

	const size_t pointLightDataSize = sizeof(PointLight) * maxPointLights;
	
	std::memcpy(pointLightBufferMemory, pointLights.data(), pointLightDataSize);

	vmaFlushAllocation(context.allocator, pointLightBufferAllocation, 0, pointLightDataSize);

	const size_t spotLightDataSize =
    sizeof(SpotLight) * maxSpotLights;

	std::memcpy(spotLightBufferMemory, spotLights.data(), spotLightDataSize);

	vmaFlushAllocation(context.allocator, spotLightBufferAllocation, 0, spotLightDataSize);

	// II. Animation preparation

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

	// III. View

	const float aspect = static_cast<float>(context.swapchain_extent.width) / static_cast<float>(context.swapchain_extent.height);
	
	// [new] camera settings!

	const float yaw = glm::radians(cameraYaw);
	const float pitch = glm::radians(cameraPitch);

	const glm::vec3 cameraDirection{
		std::cos(yaw) * std::cos(pitch),
		std::sin(pitch),
		std::sin(yaw) * std::cos(pitch),
	};

	const glm::mat4 view = glm::lookAt(
		cameraPosition,
		cameraPosition + cameraDirection,
		glm::vec3(0.0f, 1.0f, 0.0f)
	);

	glm::mat4 projection;

	if (perspectiveProjection)
	{
		projection = glm::perspective(
			glm::radians(cameraFOV),
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

	// IV. [new] double buffer fillmento

	sceneUniformBufferMemory->viewProjection = projection * view;
	sceneUniformBufferMemory->cameraPosition = glm::vec4(cameraPosition, 1.0f);
	sceneUniformBufferMemory->ambientColorIntensity = glm::vec4(ambientColor, ambient);
	sceneUniformBufferMemory->lightDirection = glm::vec4(lightDirection, 0.0f);
	sceneUniformBufferMemory->lightColorIntensity = glm::vec4(lightColor, lightIntensity);
	sceneUniformBufferMemory->lightCounts = glm::ivec4(pointLightCount, spotLightCount, 0, 0);

	vmaFlushAllocation(context.allocator, sceneUniformBufferAllocation, 0, sizeof(SceneUniformBufferObject));

	for (RenderObject& object : renderObjects)
	{
		glm::mat4 model(1.0f);

		model = glm::translate(model, animatedPos + object.position);

		const glm::vec3 rotation = animatedRot + object.rotationDegrees;

		model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		model = glm::scale(model, objectScale * object.scale);

		object.uniformBufferMemory->model = model;
		object.uniformBufferMemory->color = glm::vec4(objectColor, 1.0f);
		object.uniformBufferMemory->materialParameters = glm::vec4(shininess, 0.0f, 0.0f, 0.0f);
		object.uniformBufferMemory->specularColor = glm::vec4(specularColor, 1.0f);

		vmaFlushAllocation(context.allocator, object.uniformBufferAllocation, 0, sizeof(ModelUniformBufferObject));
	}

	// (Marker fillmento)

	const int activeMarkerCount = pointLightCount + spotLightCount;

	for (int i = 0; i < activeMarkerCount; ++i)
	{
		glm::vec3 markerPosition;
		glm::vec3 markerColor;

		if (i < pointLightCount)
		{
			markerPosition = glm::vec3(pointLights[i].position);
			markerColor = glm::vec3(pointLights[i].colorIntensity);
		}
		else
		{
			const int spotIndex = i - pointLightCount;

			markerPosition = glm::vec3(spotLights[spotIndex].position);
			markerColor = glm::vec3(spotLights[spotIndex].colorIntensity);
		}

		RenderObject& marker = lightMarkers[i];

		glm::mat4 markerModel = glm::translate(glm::mat4(1.0f), markerPosition);
		markerModel = glm::scale(markerModel, glm::vec3(lightMarkerSize));

		marker.uniformBufferMemory->model = markerModel;
		marker.uniformBufferMemory->color = glm::vec4(markerColor, 1.0f);
		marker.uniformBufferMemory->materialParameters = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
		marker.uniformBufferMemory->specularColor = glm::vec4(0.0f);

		vmaFlushAllocation(context.allocator, marker.uniformBufferAllocation, 0, sizeof(ModelUniformBufferObject));
	}

	// (SpotLines fillmento)

	for (int i = 0; i < spotLightCount; ++i)
	{
		glm::vec3 direction = glm::vec3(spotLights[i].direction);
		direction = glm::normalize(direction);

		const glm::vec3 end = direction / lightMarkerSize;
		Vertex* vertices = spotLineVertexMemory + i * spotLineVerticesPerLight;

		vertices[0] = Vertex{
			.position = {0.0f, 0.0f, 0.0f},
			.color = {1.0f, 1.0f, 1.0f},
			.normal = {0.0f, 1.0f, 0.0f},
		};

		vertices[1] = Vertex{
			.position = {end.x, end.y, end.z},
			.color = {1.0f, 1.0f, 1.0f},
			.normal = {0.0f, 1.0f, 0.0f},
		};

		glm::vec3 reference(0.0f, 1.0f, 0.0f);
		if (std::abs(direction.y) > 0.99f)
		{
			reference = glm::vec3(1.0f, 0.0f, 0.0f);
		}

		const glm::vec3 right = glm::normalize(glm::cross(direction, reference));
		const glm::vec3 up = glm::cross(right, direction);
		const float radius = std::tan(glm::radians(spotLights[i].coneAnglesDegrees.y)) / lightMarkerSize;
		uint32_t vertexIndex = 2;

		for (uint32_t j = 0; j < spotConeSegments; ++j)
		{
			const float angle = 2.0f * std::numbers::pi_v<float> * float(j) / float(spotConeSegments);
			const float nextAngle = 2.0f * std::numbers::pi_v<float> * float(j + 1) / float(spotConeSegments);

			const glm::vec3 first = end + radius * (std::cos(angle) * right + std::sin(angle) * up);
			const glm::vec3 second = end + radius * (std::cos(nextAngle) * right + std::sin(nextAngle) * up);
			vertices[vertexIndex++] = makeLightLineVertex(first);
			vertices[vertexIndex++] = makeLightLineVertex(second);

			if (j % (spotConeSegments / 4) == 0)
			{
				vertices[vertexIndex++] = makeLightLineVertex(glm::vec3(0.0f));
				vertices[vertexIndex++] = makeLightLineVertex(first);
			}
		}
	}

	vmaFlushAllocation(context.allocator, spotLineVertexAllocation, 0, VK_WHOLE_SIZE);

	// IV. Clearance and fill with dark color.

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

	// V. Main.

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
		vkCmdBindDescriptorSets(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &object.descriptorSet, 0, nullptr);
		vkCmdBindDescriptorSets(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &sceneDescriptorSet, 0, nullptr);

		vkCmdDrawIndexed(fd.command_buffer, static_cast<uint32_t>(cylinderIndices.size()), 1, 0, 0, 0);
	}

	if (showVisualization)
	{
		vkCmdBindVertexBuffers(fd.command_buffer, 0, 1, &lightMarkerVertexBuffer, &vertexBufferOffset);
		vkCmdBindIndexBuffer(fd.command_buffer, lightMarkerIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

		for (int i = 0; i < activeMarkerCount; ++i)
		{
			const RenderObject& marker = lightMarkers[i];

			vkCmdBindDescriptorSets(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &marker.descriptorSet, 0, nullptr);
			vkCmdDrawIndexed(fd.command_buffer, static_cast<uint32_t>(lightMarkerIndices.size()), 1, 0, 0, 0);
		}

		// spotLines

		vkCmdBindPipeline(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightLinePipeline);
		vkCmdBindVertexBuffers(fd.command_buffer, 0, 1, &spotLineVertexBuffer, &vertexBufferOffset);

		for (int i = 0; i < spotLightCount; ++i)
		{
			const RenderObject& marker = lightMarkers[pointLightCount + i];

			vkCmdBindDescriptorSets(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &marker.descriptorSet, 0, nullptr);
			vkCmdDraw(fd.command_buffer, spotLineVerticesPerLight, 1, static_cast<uint32_t>(i) * spotLineVerticesPerLight, 0);
		}
	}

	vkCmdEndRenderPass(fd.command_buffer);
	vkEndCommandBuffer(fd.command_buffer);
}
}

#pragma endregion