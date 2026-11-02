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
#include <lodepng.h>

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

float cameraFOV = 45.0f;
float cameraYaw = -130.0f;
float cameraPitch = -30.0f;
glm::vec3 cameraPosition(2.0f, 1.8f, 2.5f);

float shininess = 32.0f;
float ambient = 0.02f;
glm::vec3 ambientColor(1.0f);
glm::vec3 specularColor(0.8f);
glm::vec3 lightDirection(-0.5f, 1.0f, 0.3f);
glm::vec3 lightColor(1.0f);
float lightIntensity = 0.5f;

// [new] staging buffer && command pool const

VkBuffer textureStagingBuffer = VK_NULL_HANDLE;
VmaAllocation textureStagingAllocation = nullptr;
VkCommandPool uploadCommandPool = VK_NULL_HANDLE;
VkCommandBuffer uploadCommandBuffer = VK_NULL_HANDLE;

// [new] imageView&Sampler const

VkImageView textureImageView = VK_NULL_HANDLE;
VkSampler textureSampler = VK_NULL_HANDLE;

// [new] dynamic uniform buffer const

VkBuffer modelUniformBuffer = VK_NULL_HANDLE;
VmaAllocation modelUniformBufferAllocation = nullptr;
unsigned char* modelUniformBufferMemory = nullptr;
VkDeviceSize modelUniformStride = 0;

// [new] Texture push structure

struct TexturePushConstants
{
    float time = 0.0f;
    float strength = 0.5f;
	float parameter = 20.0f;
	float speed = 2.0f;
    float uvScale = 1.0f;
    uint32_t enabled = 0;
};

TexturePushConstants textureEffect;

static_assert(sizeof(TexturePushConstants) == 24);

struct Vertex {
	float position[3];
	float color[3];
	float normal[3];
	float uv[2]{0.0f, 0.0f};
};

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

// [new] TextureMaterial structs

struct TextureResource
{
    unsigned width = 0;
    unsigned height = 0;

    std::vector<unsigned char> pixels;

    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    VkImageView view = VK_NULL_HANDLE;
};

struct Material
{
    const char* name = "";
    const char* albedoPath = "";
	const char* specularPath = nullptr;
	const char* emissivePath = nullptr;

    TextureResource albedo;
	TextureResource specular;
	TextureResource emissive;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
};

std::array<Material, 6> materials{{
	{
        .name = "Soda",
        .albedoPath = "textures/can.png",
    },
    {
        .name = "Checkboard",
        .albedoPath = "textures/checkboard.png",
		.emissivePath = "textures/noise.png",
    },
    {
        .name = "Clear tux",
        .albedoPath = "textures/tux.png",
    },
	    {
        .name = "Specular tux",
        .albedoPath = "textures/tux.png",
		.specularPath = "textures/tuxSpecular.png",
    },
	    {
        .name = "Emission tux",
        .albedoPath = "textures/tux.png",
		.emissivePath = "textures/tuxEmission.png",
    },
	{
        .name = "Homyak",
        .albedoPath = "textures/homyak.png",
		.specularPath = "textures/homyakSpecular.png",
    },
}};

TextureResource blackTexture;

struct RenderObject {
	glm::vec3 position{0.0f};
	glm::vec3 rotationDegrees{0.0f};
	glm::vec3 scale{1.0f};
	uint32_t uniformBufferOffset = 0;
	int materialIndex = 0;
	bool textureEffectEnabled = false;
	float textureEffectStrength = 0.5f;
	float textureParameter = 20.0f;
	float textureSpeed = 2.0f;
	float textureUVScale = 1.0f;

	ModelUniformBufferObject* uniformBufferMemory = nullptr;
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
};

constexpr uint32_t maxPointLights = 8;

std::array<PointLight, maxPointLights> pointLights{};
int pointLightCount = 0;

constexpr uint32_t maxSpotLights = 8;

std::array<SpotLight, maxSpotLights> spotLights{};
int spotLightCount = 0;

constexpr uint32_t maxLightMarkers = maxPointLights + maxSpotLights;
std::array<RenderObject, maxLightMarkers> lightMarkers{};

constexpr uint32_t spotConeSegments = 16;
constexpr uint32_t spotLineVerticesPerLight = 2 + 2 * spotConeSegments + 8;

VkBuffer spotLineVertexBuffer = VK_NULL_HANDLE;
VmaAllocation spotLineVertexAllocation = nullptr;
Vertex* spotLineVertexMemory = nullptr;

constexpr float lightMarkerSize = 0.1f;

bool showVisualization = true;

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

VkBuffer vertexStagingBuffer = VK_NULL_HANDLE;
VmaAllocation vertexStagingAllocation = nullptr;

VkBuffer indexBuffer = VK_NULL_HANDLE;
VmaAllocation indexBufferAllocation = nullptr;

VkBuffer indexStagingBuffer = VK_NULL_HANDLE;
VmaAllocation indexStagingAllocation = nullptr;

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

	cylinderVertices.reserve(cylinderSegments * 4 + 4);
	cylinderIndices.reserve(cylinderSegments * 12);

	constexpr float radius = 0.5f;
	constexpr float height = 0.7f;

	for (uint32_t i = 0; i <= cylinderSegments; ++i)
	{
		const float angle = 2.0f * std::numbers::pi_v<float> * float(i) / float(cylinderSegments);

		const float u = float(i) / float(cylinderSegments);

		const float red = 0.5f + 0.5f * std::sin(angle);
		const float green = 0.5f + 0.5f * std::sin(angle + 2.0f * std::numbers::pi_v<float> / 3.0f);
		const float blue = 0.5f + 0.5f * std::sin(angle + 4.0f * std::numbers::pi_v<float> / 3.0f);
		
		const float x = radius * std::cos(angle);
		const float z = radius * std::sin(angle);

		cylinderVertices.push_back({
			.position = {x, -height, z},
			.color = {red, green, blue},
			.normal = {std::cos(angle), 0.0f, std::sin(angle)},
			.uv = {u, 1.0f},
		});

		cylinderVertices.push_back({
			.position = {x, height, z},
			.color = {red, green, blue},
			.normal = {std::cos(angle), 0.0f, std::sin(angle)},
			.uv = {u, 0.0f},
		});
	}

	cylinderVertices.push_back({
		.position = {0.0f, -height, 0.0f},
		.color = {0.5f, 0.5f, 0.5f},
		.normal = {0.0f, -1.0f, 0.0f},
		.uv = {0.5f, 0.5f},
	});

	cylinderVertices.push_back({
		.position = {0.0f, height, 0.0f},
		.color = {0.5f, 0.5f, 0.5f},
		.normal = {0.0f, 1.0f, 0.0f},
		.uv = {0.5f, 0.5f},
	});

	const uint32_t bottomCenter = (cylinderSegments + 1) * 2;
	const uint32_t topCenter = bottomCenter + 1;

	const uint32_t capStart = static_cast<uint32_t>(cylinderVertices.size());

	for (uint32_t i = 0; i < cylinderSegments; ++i)
	{
		const float angle = 2.0f * std::numbers::pi_v<float> * float(i) / float(cylinderSegments);

		const float red = 0.5f + 0.5f * std::sin(angle);
		const float green = 0.5f + 0.5f * std::sin(angle + 2.0f * std::numbers::pi_v<float> / 3.0f);
		const float blue = 0.5f + 0.5f * std::sin(angle + 4.0f * std::numbers::pi_v<float> / 3.0f);
		
		const float x = radius * std::cos(angle);
		const float z = radius * std::sin(angle);
		const float u = 0.5f + x / (2.0f * radius);
		const float v = 0.5f - z / (2.0f * radius);

		cylinderVertices.push_back({
			.position = {x, -height, z},
			.color = {red, green, blue},
			.normal = {0.0f, -1.0f, 0.0f},
			.uv = {u, v},
		});

		cylinderVertices.push_back({
			.position = {x, height, z},
			.color = {red, green, blue},
			.normal = {0.0f, 1.0f, 0.0f},
			.uv = {u,v},
		});
	}

	for (uint32_t i = 0; i < cylinderSegments; ++i)
	{
		const uint32_t next = (i + 1) % cylinderSegments;

		const uint32_t bottom = i * 2;
		const uint32_t top = bottom + 1;

		const uint32_t nextBottom = (i + 1) * 2;
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

bool uploadTexture(TextureResource& texture)
{
    auto& context = graphics::internal::context;

	// [new] Texture

	VkImageCreateInfo textureImageInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.extent = {texture.width, texture.height, 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	VmaAllocationCreateInfo textureAllocationInfo{
		.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
		.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	};

	if (vmaCreateImage(context.allocator, &textureImageInfo, &textureAllocationInfo, &texture.image, &texture.allocation, nullptr) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать изображение текстуры.\n";
		return false;
	}

	const VkDeviceSize textureDataSize = texture.pixels.size();

	// [new] Staging Buffer
	
	VkBufferCreateInfo stagingBufferInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = textureDataSize,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VmaAllocationCreateInfo stagingAllocationInfo{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	VmaAllocationInfo stagingMemoryInfo{};

	if (vmaCreateBuffer(context.allocator, &stagingBufferInfo, &stagingAllocationInfo, &textureStagingBuffer, &textureStagingAllocation, &stagingMemoryInfo) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать staging buffer для текстуры.\n";
		return false;
	}

	std::memcpy(stagingMemoryInfo.pMappedData, texture.pixels.data(), texture.pixels.size());

	vmaFlushAllocation(context.allocator, textureStagingAllocation, 0, textureDataSize);

	VkCommandBufferBeginInfo uploadBeginInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	if (vkResetCommandBuffer(uploadCommandBuffer, 0) != VK_SUCCESS)
	{
		std::cerr << "Не удалось сбросить command buffer загрузки.\n";
		return false;
	}

	if (vkBeginCommandBuffer(uploadCommandBuffer, &uploadBeginInfo) != VK_SUCCESS)
	{
		std::cerr << "Не удалось начать запись команд загрузки.\n";
		return false;
	}

	// [new] Barrier 1: Undefined -> Transfer_dst_optimal

	VkImageMemoryBarrier textureToTransfer{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = 0,
		.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = texture.image,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	vkCmdPipelineBarrier(uploadCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &textureToTransfer);

	VkBufferImageCopy textureCopyRegion{
		.bufferOffset = 0,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
		.imageOffset = {0, 0, 0},
		.imageExtent = {texture.width, texture.height, 1},
	};

	vkCmdCopyBufferToImage(uploadCommandBuffer, textureStagingBuffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &textureCopyRegion);

	// [new] Barrier 2: Transfer_dst_optimal -> Shader_read_only_optimal

	VkImageMemoryBarrier textureToShaderRead{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = texture.image,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	vkCmdPipelineBarrier(uploadCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &textureToShaderRead);

	if (vkEndCommandBuffer(uploadCommandBuffer) != VK_SUCCESS)
	{
		std::cerr << "Не удалось завершить запись команд загрузки.\n";
		return false;
	}

	VkFence uploadFence = VK_NULL_HANDLE;
	VkFenceCreateInfo uploadFenceInfo{
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
	};
	if (vkCreateFence(context.device, &uploadFenceInfo, nullptr, &uploadFence) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать fence загрузки.\n";
		return false;
	}

	// [new] Sending&waiting command buffer...

	VkSubmitInfo uploadSubmitInfo{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &uploadCommandBuffer,
	};

	if (vkQueueSubmit(context.graphics_queue, 1, &uploadSubmitInfo, uploadFence) != VK_SUCCESS)
	{
		std::cerr << "Не удалось отправить команды загрузки.\n";

		vkDestroyFence(context.device, uploadFence, nullptr);
		return false;
	}

	if (vkWaitForFences(context.device, 1, &uploadFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS)
	{
		std::cerr << "Не удалось дождаться завершения загрузки.\n";
		return false;
	}

	vkDestroyFence(context.device, uploadFence, nullptr);
	vmaDestroyBuffer(context.allocator, textureStagingBuffer, textureStagingAllocation);

	textureStagingBuffer = VK_NULL_HANDLE;
	textureStagingAllocation = nullptr;

	// [new] ImageView
	
	VkImageViewCreateInfo textureViewInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = texture.image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	if (vkCreateImageView(context.device, &textureViewInfo, nullptr, &texture.view) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать image view текстуры.\n";
		return false;
	}

    return true;
}

bool loadTexturePixels(TextureResource& texture, const char* path)
{
    const unsigned error = lodepng::decode(texture.pixels, texture.width, texture.height, path, LCT_RGBA, 8);

    if (error != 0)
    {
        std::cerr << "Не удалось прочитать " << path << ": " << lodepng_error_text(error) << '\n';

        return false;
    }

    return true;
}

#pragma region Initze

bool initialize() {

	// [new] Load texture

	for (Material& material : materials)
	{
		if (!loadTexturePixels(material.albedo, material.albedoPath))
		{
			return false;
		}

		if (material.specularPath != nullptr)
		{
			if (!loadTexturePixels(material.specular, material.specularPath))
			{
				return false;
			}
		}

		if (material.emissivePath != nullptr)
		{
			if (!loadTexturePixels(material.emissive, material.emissivePath))
			{
				return false;
			}
		}
	}

	blackTexture.width = 1;
	blackTexture.height = 1;
	blackTexture.pixels = {0, 0, 0, 255};

	// Cylinder(s?)

	generateCylinderGeometry();

	renderObjects[0].position = {-1.0f, 0.0f, 0.0f};
	renderObjects[1].position = {0.0f, 0.0f, 0.0f};
	renderObjects[2].position = {1.0f, 0.0f, 0.0f};

	renderObjects[0].materialIndex = 2;
	renderObjects[1].materialIndex = 3;
	renderObjects[2].materialIndex = 4;

	for (RenderObject& object : renderObjects)
	{
		object.scale = glm::vec3(0.55f);
	}

	auto& context = graphics::internal::context;

	// [new] Dynamic buffer

	VkPhysicalDeviceProperties deviceProperties{};
	vkGetPhysicalDeviceProperties(context.physical_device, &deviceProperties);
	const VkDeviceSize alignment = deviceProperties.limits.minUniformBufferOffsetAlignment;

	modelUniformStride = ((sizeof(ModelUniformBufferObject) + alignment - 1) / alignment) * alignment;

	const VkDeviceSize modelRecordCount = renderObjects.size() + lightMarkers.size();

	VkBufferCreateInfo modelBufferInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = modelUniformStride * modelRecordCount,
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VmaAllocationCreateInfo modelAllocationInfo{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	if (vmaCreateBuffer(context.allocator, &modelBufferInfo, &modelAllocationInfo, &modelUniformBuffer, &modelUniformBufferAllocation, nullptr) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать общий буфер моделей.\n";
		return false;
	}

	if (vmaMapMemory(context.allocator, modelUniformBufferAllocation, reinterpret_cast<void**>(&modelUniformBufferMemory)) != VK_SUCCESS)
	{
		std::cerr << "Не удалось отобразить память общего буфера моделей.\n";
		return false;
	}

	// [new] Command pool&buffer

	VkCommandPoolCreateInfo uploadPoolInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = context.graphics_queue_index,
	};

	if (vkCreateCommandPool(context.device, &uploadPoolInfo, nullptr, &uploadCommandPool) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать command pool загрузки.\n";
		return false;
	}

	VkCommandBufferAllocateInfo uploadCommandInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = uploadCommandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};

	if (vkAllocateCommandBuffers(context.device, &uploadCommandInfo, &uploadCommandBuffer) != VK_SUCCESS)
	{
		std::cerr << "Не удалось выделить command buffer загрузки.\n";
		return false;
	}

	// [new] Material upload + Sampler

	for (Material& material : materials)
	{
		if (!uploadTexture(material.albedo))
		{
			return false;
		}

		if (material.specularPath != nullptr)
		{
			if (!uploadTexture(material.specular))
			{
				return false;
			}
		}

		if (material.emissivePath != nullptr)
		{
			if (!uploadTexture(material.emissive))
			{
				return false;
			}
		}
	}
	
	if (!uploadTexture(blackTexture))
	{
		std::cerr << "Не удалось загрузить чёрную текстуру.\n";
		return false;
	}

	VkSamplerCreateInfo textureSamplerInfo{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.minLod = 0.0f,
		.maxLod = 0.0f,
	};

	if (vkCreateSampler(context.device, &textureSamplerInfo, nullptr, &textureSampler) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать sampler текстуры.\n";
		return false;
	}

	// [new] Staging vertex buffer

	const VkDeviceSize vertexDataSize = cylinderVertices.size() * sizeof(Vertex);

	VkBufferCreateInfo vertexStagingInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = vertexDataSize,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VmaAllocationCreateInfo vertexStagingAllocationInfo{
		.flags =
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
			| VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	VmaAllocationInfo vertexStagingMemoryInfo{};

	if (vmaCreateBuffer(context.allocator, &vertexStagingInfo, &vertexStagingAllocationInfo, &vertexStagingBuffer, &vertexStagingAllocation, &vertexStagingMemoryInfo) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать staging buffer вершин.\n";
		return false;
	}

	std::memcpy(vertexStagingMemoryInfo.pMappedData, cylinderVertices.data(), static_cast<size_t>(vertexDataSize));

	vmaFlushAllocation(context.allocator, vertexStagingAllocation, 0, vertexDataSize);

	// [new] Vertex-related Buffers

	VkCommandBufferBeginInfo uploadBeginInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	VkBufferCreateInfo vertexBufferInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = vertexDataSize,
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VmaAllocationCreateInfo vertexAllocationInfo{
		.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
		.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	};

	if (vmaCreateBuffer(context.allocator, &vertexBufferInfo, &vertexAllocationInfo, &vertexBuffer, &vertexBufferAllocation, nullptr) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать vertex buffer.\n";
		return false;
	}

	if (vkResetCommandBuffer(uploadCommandBuffer, 0) != VK_SUCCESS)
	{
		std::cerr << "Не удалось сбросить command buffer загрузки.\n";
		return false;
	}

	if (vkBeginCommandBuffer(uploadCommandBuffer, &uploadBeginInfo) != VK_SUCCESS)
	{
		std::cerr << "Не удалось начать загрузку вершин.\n";
		return false;
	}

	VkBufferCopy vertexCopyRegion{
		.srcOffset = 0,
		.dstOffset = 0,
		.size = vertexDataSize,
	};

	vkCmdCopyBuffer(uploadCommandBuffer, vertexStagingBuffer, vertexBuffer, 1, &vertexCopyRegion);

	VkMemoryBarrier vertexReadyBarrier{
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
	};

	vkCmdPipelineBarrier(uploadCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 1, &vertexReadyBarrier, 0, nullptr, 0, nullptr);

	if (vkEndCommandBuffer(uploadCommandBuffer) != VK_SUCCESS)
	{
		std::cerr << "Не удалось завершить команды загрузки вершин.\n";
		return false;
	}

	VkSubmitInfo vertexUploadSubmitInfo{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &uploadCommandBuffer,
	};

	if (vkQueueSubmit(context.graphics_queue, 1, &vertexUploadSubmitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
	{
		std::cerr << "Не удалось отправить загрузку вершин.\n";
		return false;
	}

	if (vkQueueWaitIdle(context.graphics_queue) != VK_SUCCESS)
	{
		std::cerr << "Не удалось дождаться загрузки вершин.\n";
		return false;
	}

	vmaDestroyBuffer(context.allocator, vertexStagingBuffer, vertexStagingAllocation);

	vertexStagingBuffer = VK_NULL_HANDLE;
	vertexStagingAllocation = nullptr;

	// [new]  Index-related Buffers

	const size_t indexDataSize = cylinderIndices.size() * sizeof(uint32_t);

	VkBufferCreateInfo indexStagingInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = indexDataSize,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VmaAllocationCreateInfo indexStagingAllocationInfo{
		.flags =
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	VmaAllocationInfo indexStagingMemoryInfo{};

	if (vmaCreateBuffer(context.allocator, &indexStagingInfo, &indexStagingAllocationInfo, &indexStagingBuffer, &indexStagingAllocation, &indexStagingMemoryInfo) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать staging buffer индексов.\n";
		return false;
	}

	std::memcpy(indexStagingMemoryInfo.pMappedData, cylinderIndices.data(), indexDataSize);

	vmaFlushAllocation(context.allocator, indexStagingAllocation, 0, indexDataSize);

	VkBufferCreateInfo indexBufferInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = indexDataSize,
		.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT
			| VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VmaAllocationCreateInfo indexAllocationInfo{
		.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
		.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	};

	if (vmaCreateBuffer(context.allocator, &indexBufferInfo, &indexAllocationInfo, &indexBuffer, &indexBufferAllocation, nullptr) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать index buffer.\n";
		return false;
	}

	if (vkResetCommandBuffer(uploadCommandBuffer, 0) != VK_SUCCESS)
	{
		std::cerr << "Не удалось сбросить command buffer загрузки.\n";
		return false;
	}

	if (vkBeginCommandBuffer(uploadCommandBuffer, &uploadBeginInfo) != VK_SUCCESS)
	{
		std::cerr << "Не удалось начать загрузку индексов.\n";
		return false;
	}

	VkBufferCopy indexCopyRegion{
		.srcOffset = 0,
		.dstOffset = 0,
		.size = indexDataSize,
	};

	vkCmdCopyBuffer(uploadCommandBuffer, indexStagingBuffer, indexBuffer, 1, &indexCopyRegion);

	VkMemoryBarrier indexReadyBarrier{
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_INDEX_READ_BIT,
	};

	vkCmdPipelineBarrier(uploadCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 1, &indexReadyBarrier, 0, nullptr, 0, nullptr);

	if (vkEndCommandBuffer(uploadCommandBuffer) != VK_SUCCESS)
	{
		std::cerr << "Не удалось завершить команды загрузки индексов.\n";
		return false;
	}

	VkSubmitInfo indexUploadSubmitInfo{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &uploadCommandBuffer,
	};

	if (vkQueueSubmit(context.graphics_queue, 1, &indexUploadSubmitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
	{
		std::cerr << "Не удалось отправить загрузку индексов.\n";
		return false;
	}

	if (vkQueueWaitIdle(context.graphics_queue) != VK_SUCCESS)
	{
		std::cerr << "Не удалось дождаться загрузки индексов.\n";
		return false;
	}

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

	uint32_t modelRecordIndex = 0;

	for (RenderObject& object : renderObjects)
	{
		object.uniformBufferOffset = static_cast<uint32_t>(modelUniformStride * modelRecordIndex);
		object.uniformBufferMemory = reinterpret_cast<ModelUniformBufferObject*>(modelUniformBufferMemory + object.uniformBufferOffset);
		++modelRecordIndex;
	}

	for (RenderObject& marker : lightMarkers)
	{
		marker.uniformBufferOffset = static_cast<uint32_t>(modelUniformStride * modelRecordIndex);
		marker.uniformBufferMemory = reinterpret_cast<ModelUniformBufferObject*>(modelUniformBufferMemory + marker.uniformBufferOffset);
		++modelRecordIndex;
	}

	// PointLight

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

	// SpotLight

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

	// LightMarker
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

	VkDescriptorSetLayoutBinding albedoLayoutBinding{
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
	};

	VkDescriptorSetLayoutBinding modelUniformLayoutBinding = uniformLayoutBinding;

	modelUniformLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;

	VkDescriptorSetLayoutBinding specularLayoutBinding = albedoLayoutBinding;
	specularLayoutBinding.binding = 2;

	VkDescriptorSetLayoutBinding emissiveLayoutBinding = albedoLayoutBinding;
	emissiveLayoutBinding.binding = 3;

	VkDescriptorSetLayoutBinding modelBindings[]{
		modelUniformLayoutBinding,
		albedoLayoutBinding,
		specularLayoutBinding,
		emissiveLayoutBinding,
	};

	VkDescriptorSetLayoutCreateInfo modelLayoutInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 4,
		.pBindings = modelBindings,
	};

	if (vkCreateDescriptorSetLayout(context.device, &modelLayoutInfo, nullptr, &modelDescriptorSetLayout) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать descriptor layout модели.\n";
		return false;
	}

	// Descriptor pool

	const uint32_t descriptorSetCapacity = 1 + static_cast<uint32_t>(materials.size() + lightMarkers.size());

	VkDescriptorPoolSize poolSizes[]{
		{
			.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = descriptorSetCapacity,
		},
		{
			.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 4,
		},
		{
			.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 3 * descriptorSetCapacity,
		},
		{
			.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			.descriptorCount = static_cast<uint32_t>(materials.size() + lightMarkers.size()),
		},
	};

	VkDescriptorPoolCreateInfo descriptorPoolInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = descriptorSetCapacity,
		.poolSizeCount = 4,
		.pPoolSizes = poolSizes,
	};

	if (vkCreateDescriptorPool(context.device, &descriptorPoolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать descriptor pool.\n";
		return false;
	}

	VkDescriptorImageInfo albedoImageInfo{
		.sampler = textureSampler,
		.imageView = materials[0].albedo.view,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	// Descriptor set

	for (Material& material : materials)
	{
		VkDescriptorSetAllocateInfo materialSetInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = descriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts = &modelDescriptorSetLayout,
		};

		if (vkAllocateDescriptorSets(
				context.device,
				&materialSetInfo,
				&material.descriptorSet) != VK_SUCCESS)
		{
			std::cerr << "Не удалось выделить набор материала.\n";
			return false;
		}

		VkDescriptorBufferInfo modelBufferDescriptor{
			.buffer = modelUniformBuffer,
			.offset = 0,
			.range = sizeof(ModelUniformBufferObject),
		};

		VkDescriptorImageInfo materialImageInfo{
			.sampler = textureSampler,
			.imageView = material.albedo.view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};

		VkDescriptorImageInfo specularImageInfo{
			.sampler = textureSampler,
			.imageView = material.specularPath != nullptr ? material.specular.view : blackTexture.view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};

		VkDescriptorImageInfo emissiveImageInfo{
			.sampler = textureSampler,
			.imageView = material.emissivePath != nullptr ? material.emissive.view : blackTexture.view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};

		VkWriteDescriptorSet materialWrites[]{
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = material.descriptorSet,
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
				.pBufferInfo = &modelBufferDescriptor,
			},
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = material.descriptorSet,
				.dstBinding = 1,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &materialImageInfo,
			},
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = material.descriptorSet,
				.dstBinding = 2,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &specularImageInfo,
			},
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = material.descriptorSet,
				.dstBinding = 3,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &emissiveImageInfo,
			},
		};

		vkUpdateDescriptorSets(context.device, 4, materialWrites, 0, nullptr);
	}

	VkDescriptorImageInfo blackImageInfo{
		.sampler = textureSampler,
		.imageView = blackTexture.view,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

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
			.buffer = modelUniformBuffer,
			.offset = 0,
			.range = sizeof(ModelUniformBufferObject),
		};

		VkWriteDescriptorSet markerWrite{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = marker.descriptorSet,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			.pBufferInfo = &markerBufferInfo,
		};

		vkUpdateDescriptorSets(context.device, 1, &markerWrite, 0, nullptr);

		VkWriteDescriptorSet albedoWrite{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = marker.descriptorSet,
			.dstBinding = 1,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &albedoImageInfo,
		};

		vkUpdateDescriptorSets(context.device, 1, &albedoWrite, 0, nullptr);

		for (uint32_t binding = 2; binding <= 3; ++binding)
		{
			VkWriteDescriptorSet blackWrite{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = marker.descriptorSet,
				.dstBinding = binding,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &blackImageInfo,
			};

			vkUpdateDescriptorSets(context.device, 1, &blackWrite, 0, nullptr);
		}
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

	VkPushConstantRange texturePushRange{
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.offset = 0,
		.size = sizeof(TexturePushConstants),
	};

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 2;
	layoutInfo.pSetLayouts = setLayouts;
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &texturePushRange;

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

	VkVertexInputAttributeDescription vertexAttributes[4]{
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
		},
		{
			.location = 3,
			.binding = 0,
			.format = VK_FORMAT_R32G32_SFLOAT,
			.offset = offsetof(Vertex, uv),
		},
	};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &vertexBinding;

	vertexInputInfo.vertexAttributeDescriptionCount = 4;
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
	colorAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
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

	// The pipelines

	if (vkCreateGraphicsPipelines(graphics::internal::context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать графический pipeline.\n";
		return false;
	}

	inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

	if (vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &lightLinePipeline) != VK_SUCCESS)
	{
		std::cerr << "Не удалось создать pipeline линий света.\n";
		return false;
	}

	return true;
}

#pragma endregion

void destroyTexture(TextureResource& texture)
{
    auto& context = graphics::internal::context;

    if (texture.view != VK_NULL_HANDLE)
    {
        vkDestroyImageView(context.device, texture.view, nullptr);
        texture.view = VK_NULL_HANDLE;
    }

    if (texture.image != VK_NULL_HANDLE)
    {
        vmaDestroyImage(
            context.allocator,
            texture.image,
            texture.allocation);

        texture.image = VK_NULL_HANDLE;
        texture.allocation = nullptr;
    }
}

#pragma region Shdown

void shutdown() {
	auto& context = graphics::internal::context;
	vkQueueWaitIdle(context.graphics_queue);

	vmaDestroyBuffer(context.allocator, indexStagingBuffer, indexStagingAllocation);

	indexStagingBuffer = VK_NULL_HANDLE;
	indexStagingAllocation = nullptr;

	if (indexStagingBuffer != VK_NULL_HANDLE)
	{
		vmaDestroyBuffer(context.allocator, indexStagingBuffer, indexStagingAllocation);
		indexStagingBuffer = VK_NULL_HANDLE;
		indexStagingAllocation = nullptr;
	}

	if (vertexStagingBuffer != VK_NULL_HANDLE)
	{
		vmaDestroyBuffer(context.allocator, vertexStagingBuffer, vertexStagingAllocation);
		vertexStagingBuffer = VK_NULL_HANDLE;
		vertexStagingAllocation = nullptr;
	}

	if (modelUniformBufferMemory != nullptr)
	{
		vmaUnmapMemory(context.allocator, modelUniformBufferAllocation);
		modelUniformBufferMemory = nullptr;
	}

	if (modelUniformBuffer != VK_NULL_HANDLE)
	{
		vmaDestroyBuffer(context.allocator, modelUniformBuffer, modelUniformBufferAllocation);

		modelUniformBuffer = VK_NULL_HANDLE;
		modelUniformBufferAllocation = nullptr;
	}

	vkDestroySampler(context.device, textureSampler, nullptr);


	vkDestroyCommandPool(context.device, uploadCommandPool, nullptr);

	if (textureStagingBuffer != VK_NULL_HANDLE)
	{
		vmaDestroyBuffer(context.allocator, textureStagingBuffer, textureStagingAllocation);
	}

	for (Material& material : materials)
	{
		destroyTexture(material.albedo);
		destroyTexture(material.specular);
		destroyTexture(material.emissive);
	}

	destroyTexture(blackTexture);

	vmaDestroyBuffer(context.allocator, spotLineVertexBuffer, spotLineVertexAllocation);

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

	for (RenderObject& object : renderObjects)
	{
		object.uniformBufferMemory = nullptr;
	}

	for (RenderObject& marker : lightMarkers)
	{
		marker.uniformBufferMemory = nullptr;
	}

	vkDestroyDescriptorPool(context.device, descriptorPool, nullptr);

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

	// 0) Animated texture

	textureEffect.time = static_cast<float>(time);

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

	ImGui::Begin("Materials");

	for (size_t i = 0; i < renderObjects.size(); ++i)
	{
		ImGui::PushID(static_cast<int>(i));

		RenderObject& object = renderObjects[i];

		ImGui::Text("Cylinder %d", static_cast<int>(i + 1));

		if (ImGui::BeginCombo("Material", materials[object.materialIndex].name))
		{
			for (size_t j = 0; j < materials.size(); ++j)
			{
				const bool selected =
					object.materialIndex == static_cast<int>(j);

				if (ImGui::Selectable(materials[j].name, selected))
				{
					object.materialIndex = static_cast<int>(j);
				}
			}

			ImGui::EndCombo();
		}

		ImGui::Checkbox("Texture effect", &object.textureEffectEnabled);

		ImGui::SliderFloat("Effect strength", &object.textureEffectStrength, 0.0f, 1.0f);

		ImGui::SliderFloat("Effect speed", &object.textureSpeed, 0.0f, 10.0f);

		ImGui::SliderFloat("Effect parameter", &object.textureParameter, 1.0f, 200.0f);

		ImGui::SliderFloat("UV scale", &object.textureUVScale, 0.5f, 5.0f);
		
		ImGui::Separator();

		ImGui::PopID();
	}

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

	// I. PointLight && SpotLight

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
	
	// Camera settings!

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

	// IV. double buffer fillmento

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
	
	vmaFlushAllocation(context.allocator, modelUniformBufferAllocation, 0, VK_WHOLE_SIZE);

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

	for (size_t i = 0; i < renderObjects.size(); ++i)
	{
		const RenderObject& object = renderObjects[i];
			
		const Material& material = materials[object.materialIndex];

		vkCmdBindDescriptorSets(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &material.descriptorSet, 1, &object.uniformBufferOffset);
		vkCmdBindDescriptorSets(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &sceneDescriptorSet, 0, nullptr);

		TexturePushConstants objectEffect{
			.time = textureEffect.time,
			.strength = object.textureEffectStrength,
			.parameter = object.textureParameter,
			.speed = object.textureSpeed,
			.uvScale = object.textureUVScale,
			.enabled = object.textureEffectEnabled ? 1u : 0u,
		};

		vkCmdPushConstants(fd.command_buffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(objectEffect), &objectEffect);
			
		vkCmdDrawIndexed(fd.command_buffer, static_cast<uint32_t>(cylinderIndices.size()), 1, 0, 0, 0);
	}

	if (showVisualization)
	{
		vkCmdBindVertexBuffers(fd.command_buffer, 0, 1, &lightMarkerVertexBuffer, &vertexBufferOffset);
		vkCmdBindIndexBuffer(fd.command_buffer, lightMarkerIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

		for (int i = 0; i < activeMarkerCount; ++i)
		{
			const RenderObject& marker = lightMarkers[i];

			vkCmdBindDescriptorSets(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &marker.descriptorSet, 1, &marker.uniformBufferOffset);
			vkCmdDrawIndexed(fd.command_buffer, static_cast<uint32_t>(lightMarkerIndices.size()), 1, 0, 0, 0);
		}

		// spotLines

		vkCmdBindPipeline(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightLinePipeline);
		vkCmdBindVertexBuffers(fd.command_buffer, 0, 1, &spotLineVertexBuffer, &vertexBufferOffset);

		for (int i = 0; i < spotLightCount; ++i)
		{
			const RenderObject& marker = lightMarkers[pointLightCount + i];

			vkCmdBindDescriptorSets(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &marker.descriptorSet, 1, &marker.uniformBufferOffset);
			vkCmdDraw(fd.command_buffer, spotLineVerticesPerLight, 1, static_cast<uint32_t>(i) * spotLineVerticesPerLight, 0);
		}
	}

	vkCmdEndRenderPass(fd.command_buffer);
	vkEndCommandBuffer(fd.command_buffer);
}
}

#pragma endregion