
// A definir en premier lieu
#if defined(_WIN32)
// a definir globalement pour etre pris en compte par volk.c
//#define VK_USE_PLATFORM_WIN32_KHR

// si glfw
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#elif defined(_GNUC_)
#endif

#if defined(_MSC_VER)
//
// Pensez a copier les dll dans le repertoire x64/Debug, cad:
// glfw-3.3/lib-vc2015/glfw3.dll
//
#pragma comment(lib, "glfw3dll.lib")
#pragma comment(lib, "vulkan-1.lib")
#endif

#if defined(_DEBUG)
#define VULKAN_ENABLE_VALIDATION
#endif

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#define SHADOWMAP_DIM 2048
#define SHADOWMAP_FILTER VK_FILTER_LINEAR

#define SHADOWMAP_CASCADE 4

#include "volk/volk.h"
//#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <glm\glm\glm.hpp>
#include <glm\glm\gtc\matrix_transform.hpp>


#include <imgui-master\imgui.h>
#include <imgui-master\examples\imgui_impl_glfw.h>
#include <imgui-master\examples\imgui_impl_vulkan.h>


#define STB_IMAGE_IMPLEMENTATION
#include <stb\stb_image.h>


#include <iostream>
#include <vector>
#include <array>
#include <fstream>
#include <chrono>

#ifdef _DEBUG
#define DEBUG_CHECK_VK(x) if (VK_SUCCESS != (x)) { std::cout << (#x) << std::endl; __debugbreak(); }
#else
#define DEBUG_CHECK_VK(x) 
#endif

struct Vertex
{
	glm::vec3 m_position;
	glm::vec2 m_uv;
	glm::vec3 m_normal;

	static VkVertexInputBindingDescription GetBindingDescription() 
	{ 
		VkVertexInputBindingDescription bindingDescription = {}; 
		{
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(Vertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		}
		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions() 
	{
		std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};
		{
			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[0].offset = offsetof(Vertex, m_position);

			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[1].offset = offsetof(Vertex, m_uv);

			attributeDescriptions[2].binding = 0;
			attributeDescriptions[2].location = 2;
			attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[2].offset = offsetof(Vertex, m_normal);
		}
		return attributeDescriptions;
	}
};

// CUBE
/*
const std::vector<Vertex> vertices = 
{
	{ { 0.0f, 0.0f, 0.0f }		,{ 0.0f, 0.0f, 1.0f } },
	{ { 0.0f, 0.0f, 1.0f }		,{ 0.0f, 0.0f, 1.0f } },
	{ { 0.0f, 1.0f, 0.0f }		,{ 0.0f, 0.0f, 1.0f } },
	{ { 0.0f, 1.0f, 1.0f }		,{ 0.0f, 0.0f, 1.0f } },

	{ { 1.0f, 0.0f, 0.0f }		,{ 0.0f, 1.0f, 0.0f } },
	{ { 1.0f, 0.0f, 1.0f }		,{ 0.0f, 1.0f, 0.0f } },
	{ { 1.0f, 1.0f, 0.0f }		,{ 0.0f, 1.0f, 0.0f } },
	{ { 1.0f, 1.0f, 1.0f }		,{ 0.0f, 1.0f, 0.0f } }
};

const std::vector<uint16_t> indices = 
{
	
	// FRONT S1
	4, 6, 0, 
	6, 2, 0,
	// BACK S2
	2, 3, 0, 
	3, 1, 0,
	// S3
	6, 7, 2, 
	7, 3, 2,
	// S4
	7, 6, 4, 
	5, 7, 4,
	// S5
	5, 4, 0, 
	1, 5, 0,
	// S6
	7, 5, 1, 
	3, 7, 1 
};
*/

struct VP
{
	glm::mat4 view;
	glm::mat4 proj;

	glm::mat4 lightSpace[SHADOWMAP_CASCADE];
	float cascadeSplits[SHADOWMAP_CASCADE];
	glm::vec3 lightPos;
};

struct Model
{
	glm::mat4 model;
	glm::vec4 cascadeIdx;
};

struct VulkanDeviceContext
{
	static const int MAX_DEVICE_COUNT = 9;	// arbitraire, max = IGP + 2x4 GPU max
	static const int MAX_FAMILY_COUNT = 4;	// graphics, compute, transfer, graphics+compute (ajouter sparse aussi...)
	static const int SWAPCHAIN_IMAGES = 2;

	VkDebugReportCallbackEXT m_debugCallback;
	
	VkInstance m_instance;
	VkPhysicalDevice m_physicalDevice;
	VkDevice m_device;

	VkSurfaceKHR m_surface;
	VkSurfaceFormatKHR m_surfaceFormat;
	VkSwapchainKHR m_swapchain; 
	VkExtent2D m_swapchainExtent;
	VkImage m_swapchainImages[SWAPCHAIN_IMAGES];
	VkImageView m_swapChainImageViews[SWAPCHAIN_IMAGES];

	VkPhysicalDeviceProperties m_props;
	std::vector<VkMemoryPropertyFlags> memoryFlags;

	VkSemaphore m_aquireSemaphores[SWAPCHAIN_IMAGES];
	VkSemaphore m_presentSemaphores[SWAPCHAIN_IMAGES];

	//// SHADOW ////
	VkExtent3D m_shadowExtent;



	uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
		int i = 0;
		for (auto propertyFlags : memoryFlags) {
			if ((typeFilter & (1 << i)) && (propertyFlags & properties) == properties) {
				return i;
			}
			i++;
		}

		std::cout << "failed to find suitable memory type!" << std::endl;
		return 0;
	}
	
	VkFormat FindSupportedFormat(const std::vector<VkFormat>& candiates, VkImageTiling tiling, VkFormatFeatureFlags features)
	{
		for (VkFormat format : candiates)
		{
			VkFormatProperties properties;
			vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &properties);

			if ((tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features) || (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features))
				return format;
		}

		throw std::runtime_error("aucun des formats demandés n'est surpporté!");
	}

	VkFormat FindDepthFormat()
	{
		return FindSupportedFormat(
			{VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
	}

	bool HasStencilComponent(VkFormat format)
	{
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	void CreateImage(VkFormat format, VkImageTiling tiling, VkImageUsageFlags usageFlags, VkMemoryPropertyFlags properties, const VkExtent3D& extent, int arrayLayers, VkImage& image, VkDeviceMemory& imageMemory)
	{
		VkImageCreateInfo imageInfo = {};
		{
			imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.imageType = VK_IMAGE_TYPE_2D;
			imageInfo.flags = 0;
			imageInfo.extent = extent;
			imageInfo.mipLevels = 1;
			imageInfo.arrayLayers = arrayLayers;
			imageInfo.format = format;
			imageInfo.tiling = tiling;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageInfo.usage = usageFlags;
			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		DEBUG_CHECK_VK(vkCreateImage(m_device, &imageInfo, nullptr, &image));

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(m_device,image, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		{
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = memRequirements.size;
			allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);
		}

		DEBUG_CHECK_VK(vkAllocateMemory(m_device, &allocInfo, nullptr,&imageMemory));
		DEBUG_CHECK_VK(vkBindImageMemory(m_device, image, imageMemory,0));
	}
};

struct RenderSurface
{
	VkDeviceMemory m_memory;
	VkImage m_image;
	VkImageView m_imageView;
	VkFormat m_format;

	VkSampler m_shadowSampler;
};

struct Buffer
{
	VkBuffer	m_vertexBuffer;
	VkDeviceMemory m_vertexBufferMemory;

	VkBuffer m_indexBuffer;
	VkDeviceMemory m_indexBufferMemory;

	// size + nb d'element 
	static void CreateBuffer(VulkanDeviceContext& context, VkDeviceSize size, uint32_t elementNbr, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags propertiesFlags, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
	{
		VkBufferCreateInfo bufferInfo = {};
		{
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = size * elementNbr;
			bufferInfo.usage = usageFlags;
			//bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		DEBUG_CHECK_VK(vkCreateBuffer(context.m_device, &bufferInfo, nullptr, &buffer));

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(context.m_device, buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		{
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = memRequirements.size * elementNbr;
			allocInfo.memoryTypeIndex = context.FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		}

		DEBUG_CHECK_VK(vkAllocateMemory(context.m_device, &allocInfo, nullptr, &bufferMemory));
		DEBUG_CHECK_VK(vkBindBufferMemory(context.m_device, buffer, bufferMemory, 0));
	}

	void CreateVertexBuffer(VulkanDeviceContext& context, const std::vector<Vertex> vertices)
	{
		VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

		CreateBuffer(context, bufferSize, 1, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_vertexBuffer, m_vertexBufferMemory);

		void* data;
		vkMapMemory(context.m_device, m_vertexBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, vertices.data(), (size_t)bufferSize);
		vkUnmapMemory(context.m_device, m_vertexBufferMemory);
	}

	void CreateIndexBuffer(VulkanDeviceContext& context, std::vector<uint32_t> indices)
	{
		VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

		CreateBuffer(context, bufferSize, 1, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_indexBuffer, m_indexBufferMemory);

		void* data;
		vkMapMemory(context.m_device, m_indexBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, indices.data(), (size_t)bufferSize);
		vkUnmapMemory(context.m_device, m_indexBufferMemory);
	}

	static void CreateUniformBuffers(VulkanDeviceContext& context, VkDeviceSize bufferSize, uint32_t elementNbr, std::vector<VkBuffer>& uniformBuffers, std::vector<VkDeviceMemory>& uniformBuffersMemory)
	{
		//VkDeviceSize bufferSize = sizeof(MVP);

		uniformBuffers.resize(context.SWAPCHAIN_IMAGES);
		uniformBuffersMemory.resize(context.SWAPCHAIN_IMAGES);

		for (size_t i = 0; i < context.SWAPCHAIN_IMAGES; i++)
			CreateBuffer(context, bufferSize, elementNbr, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
	}

	static void UpdateUniformBuffer(const VulkanDeviceContext& context, uint32_t currentImage, uint32_t size, uint32_t offset, std::vector<VkDeviceMemory>& uniformBuffersMemory, const void* model)
	{
		/*static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
		*/
		//VP vp = {};
		//mvp.model = glm::rotate(glm::mat4(1.0f), (time * glm::radians(10.0f)), glm::vec3(0.0f, 1.0f, 0.0f));
		//mvp.model += glm::translate(mvp.model, position);


		//vp.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		//vp.proj = glm::perspective(45.0f, context.m_swapchainExtent.width / (float)context.m_swapchainExtent.height, 0.1f, 100.0f);

		void* data;
		vkMapMemory(context.m_device, uniformBuffersMemory[currentImage], offset, size, 0, &data);
		memcpy(data, model, size);
		vkUnmapMemory(context.m_device, uniformBuffersMemory[currentImage]);
	}

	void Clean(const VkDevice device)
	{
		vkDestroyBuffer(device, m_indexBuffer, nullptr);
		vkFreeMemory(device, m_indexBufferMemory, nullptr);

		vkDestroyBuffer(device, m_vertexBuffer, nullptr);
		vkFreeMemory(device, m_vertexBufferMemory, nullptr);
	}
};

struct VulkanRenderContext
{
	static const int PENDING_FRAMES = 2;

	uint32_t m_graphicsQueueIndex;
	uint32_t m_presentQueueIndex;
	VkQueue m_graphicsQueue;
	VkQueue m_presentQueue;

	VkCommandPool m_mainCommandPool;
	VkCommandBuffer m_mainCommandBuffers[PENDING_FRAMES];
	

	VkFence m_mainFences[PENDING_FRAMES];
	VkRenderPass m_renderPass;
	VkFramebuffer m_frameBuffers[PENDING_FRAMES];
	
	std::vector<VkBuffer> m_uniformBuffers;
	std::vector<VkDeviceMemory> m_uniformBuffersMemory;

	std::vector<VkBuffer> m_uniformBuffersDynamic;
	std::vector<VkDeviceMemory> m_uniformBuffersMemoryDynamic;


	VkImageSubresourceRange m_mainSubRange;

	RenderSurface m_depthBuffer;

	//// SHADOW ////
	RenderSurface m_shadowBuffer;
	VkRenderPass m_shadowRenderPass;
	VkFramebuffer m_shadowFrameBuffer;

	std::vector<VkBuffer> m_shadowUniformBuffers;
	std::vector<VkDeviceMemory> m_shadowUniformBuffersMemory;

	std::vector<VkBuffer> m_shadowUniformBuffersDynamic;
	std::vector<VkDeviceMemory> m_shadowUniformBuffersMemoryDynamic;
};


struct Texture
{
	int m_width;
	int m_height;
	int m_channels;

	stbi_uc* m_pixels;
	VkDeviceSize m_imageSize;

	VkBuffer m_buffer;
	VkDeviceMemory m_bufferMemory;

	VkImage m_image;
	VkDeviceMemory m_imageMemory;

	VkImageView m_imageView;

	VkSampler m_sampler;

#pragma region TEXTURE_IMAGE 
	void CreateTextureColor(VulkanRenderContext& rendercontext, VulkanDeviceContext& context)
	{
		m_pixels = stbi_load("textures/texture.jpg", &m_width, &m_height, &m_channels, STBI_rgb_alpha);

		m_imageSize = m_width * m_height * 4; // 4 -> organisé par ligne de 4
		if (!m_pixels)
			throw std::runtime_error("Fail to load image");

		Buffer::CreateBuffer(context, m_imageSize, 1, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_buffer, m_bufferMemory);

		void* data;
		vkMapMemory(context.m_device, m_bufferMemory, 0, m_imageSize, 0, &data);
		memcpy(data, m_pixels, static_cast<size_t>(m_imageSize));
		vkUnmapMemory(context.m_device, m_bufferMemory);

		stbi_image_free(m_pixels);

		CreateImageTexture(rendercontext, context);
	}

	void CreateImageTexture(const VulkanRenderContext& rendercontext, VulkanDeviceContext context)
	{
		VkImageCreateInfo imageInfo = {};
		{
			imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.imageType = VK_IMAGE_TYPE_2D;
			imageInfo.extent.width = static_cast<uint32_t>(m_width);
			imageInfo.extent.height = static_cast<uint32_t>(m_height);
			imageInfo.extent.depth = 1;
			imageInfo.mipLevels = 1;
			imageInfo.arrayLayers = 1;
			imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
			imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		}

		DEBUG_CHECK_VK(vkCreateImage(context.m_device, &imageInfo, nullptr, &m_image));

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(context.m_device, m_image, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		{
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = memRequirements.size;
			allocInfo.memoryTypeIndex = context.FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		}

		DEBUG_CHECK_VK(vkAllocateMemory(context.m_device, &allocInfo,nullptr, &m_imageMemory));

		vkBindImageMemory(context.m_device, m_image,m_imageMemory,0);

		TransitionImageLayout(rendercontext, m_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		CopyBufferToImage(rendercontext, m_buffer, m_image);
		TransitionImageLayout(rendercontext, m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	void TransitionImageLayout(const VulkanRenderContext& rendercontext, VkImage& image, VkImageLayout oldLayout, VkImageLayout newLayout)
	{
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		vkBeginCommandBuffer(rendercontext.m_mainCommandBuffers[0], &beginInfo);

		CreateBarrierPipeline(rendercontext, image, oldLayout, newLayout);

		VkSubmitInfo end_info = {};
		end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		end_info.commandBufferCount = 1;
		end_info.pCommandBuffers = &rendercontext.m_mainCommandBuffers[0];

		vkEndCommandBuffer(rendercontext.m_mainCommandBuffers[0]);
		DEBUG_CHECK_VK(vkQueueSubmit(rendercontext.m_graphicsQueue, 1, &end_info, VK_NULL_HANDLE));
		vkQueueWaitIdle(rendercontext.m_graphicsQueue);
	}

	//VK_IMAGE_LAYOUT_UNDEFINED;
	void CreateBarrierPipeline(const VulkanRenderContext& rendercontext, VkImage& image, VkImageLayout oldLayout, VkImageLayout newLayout)
	{
		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;

		VkImageMemoryBarrier imageBarrier = {};
		{
			imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageBarrier.oldLayout = oldLayout;
			imageBarrier.newLayout = newLayout;
			imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageBarrier.image = image;
			imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBarrier.subresourceRange.baseMipLevel = 0;
			imageBarrier.subresourceRange.levelCount = 1;
			imageBarrier.subresourceRange.baseArrayLayer = 0;
			imageBarrier.subresourceRange.layerCount = 1;

			if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
			{
				imageBarrier.srcAccessMask = 0;
				imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

				sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
				destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			}
			else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
				imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

				sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
				destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			}
			else {
				throw std::invalid_argument("transition d'orgisation non supportée!");
			}
		}

		vkCmdPipelineBarrier(rendercontext.m_mainCommandBuffers[0], sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
	}

	void CopyBufferToImage(const VulkanRenderContext& rendercontext, VkBuffer& buffer, VkImage& image)
	{
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		vkBeginCommandBuffer(rendercontext.m_mainCommandBuffers[0], &beginInfo);

		VkBufferImageCopy region = {};
		{
			region.bufferOffset = 0;
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.mipLevel = 0;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = 1;
			region.imageOffset = { 0,0,0 };
			region.imageExtent.width = m_width;
			region.imageExtent.height = m_height;
			region.imageExtent.depth = 1;
		}

		vkCmdCopyBufferToImage(rendercontext.m_mainCommandBuffers[0], buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		VkSubmitInfo end_info = {};
		end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		end_info.commandBufferCount = 1;
		end_info.pCommandBuffers = &rendercontext.m_mainCommandBuffers[0];

		vkEndCommandBuffer(rendercontext.m_mainCommandBuffers[0]);
		DEBUG_CHECK_VK(vkQueueSubmit(rendercontext.m_graphicsQueue, 1, &end_info, VK_NULL_HANDLE));
		vkQueueWaitIdle(rendercontext.m_graphicsQueue);
	}
#pragma endregion

#pragma region TEXTURE_IMAGE_VIEW
	void CreateTextureImageView(VulkanDeviceContext& context)
	{
		m_imageView = CreateImageView(context, m_image, VK_FORMAT_R8G8B8A8_UNORM);
		CreateImageViews(context);
	}

	void CreateImageViews(VulkanDeviceContext& context)
	{
		for (uint32_t idx = 0; idx < context.SWAPCHAIN_IMAGES; idx++)
			context.m_swapChainImageViews[idx] = CreateImageView(context, context.m_swapchainImages[idx], VK_FORMAT_B8G8R8A8_SRGB);
	}

	//VK_FORMAT_R8G8B8A8_UNORM
	VkImageView CreateImageView(const VulkanDeviceContext& context, VkImage image, VkFormat format)
	{
		VkComponentMapping componentMapping = {};
		{
			componentMapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			componentMapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			componentMapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			componentMapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		}

		VkImageViewCreateInfo viewInfo = {};
		{
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = image;
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = format;
			viewInfo.components = componentMapping;

			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;
		}

		VkImageView imageView;
		DEBUG_CHECK_VK(vkCreateImageView(context.m_device, &viewInfo, nullptr, &imageView));

		return imageView;
	}

#pragma endregion

#pragma region TEXTURE_SAMPLER

	void CreateTextureSampler(const VulkanDeviceContext context)
	{
		VkSamplerCreateInfo samplerInfo = {};
		{
			samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerInfo.magFilter = VK_FILTER_LINEAR;
			samplerInfo.minFilter = VK_FILTER_LINEAR;

			samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

			samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
			samplerInfo.unnormalizedCoordinates = VK_FALSE;
			samplerInfo.compareEnable = VK_FALSE;
			samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

			samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerInfo.mipLodBias = 0.0f;
			samplerInfo.minLod = 0.0f;
			samplerInfo.maxLod = 0.0f;

			samplerInfo.anisotropyEnable = VK_FALSE;
			samplerInfo.maxAnisotropy = 1;
		}

		DEBUG_CHECK_VK(vkCreateSampler(context.m_device, &samplerInfo, nullptr, &m_sampler));
	}

#pragma endregion 

	/*bool IsDeviceSuitable(const VulkanDeviceContext context)
	{
		VkPhysicalDeviceFeatures supportedFeatures;
		vkGetPhysicalDeviceFeatures(context.m_physicalDevice, &supportedFeatures);

		return 
	}*/

	void Clean(const VulkanDeviceContext& context)
	{
		vkDestroySampler(context.m_device, m_sampler, nullptr);
		vkDestroyImageView(context.m_device, m_imageView, nullptr);

		vkDestroyImage(context.m_device, m_image, nullptr);
		vkFreeMemory(context.m_device, m_imageMemory, nullptr);
	}
};

struct Mesh
{
	std::vector<Vertex> m_vertices;
	std::vector<uint32_t> m_indices;

	Model m_model;

	Buffer m_buffer;

	Texture m_texture;
};

struct Scene
{
	std::vector<Mesh> m_meshes;
};

struct Cascade
{
	VkFramebuffer		m_frameBuffer;
	VkDescriptorSet		m_descriptorSet;
	VkImageView			m_imageView;
};

struct VulkanGraphiquePipeline
{
	VkPipelineLayout m_pipelineLayout;
	VkGraphicsPipelineCreateInfo m_pipelineInfo;
	
	VkDescriptorSetLayout m_descriptorSetLayout;
	VkDescriptorPool m_descriptorPool;
	std::vector<VkDescriptorSet> m_descriptorSets;

	Scene m_scene;
	
	VkPipeline m_pipelineOpaque;

	// SHADOW
	VkPipelineLayout m_shadowPipelineLayout;
	VkGraphicsPipelineCreateInfo m_shadowPipelineInfo;
	VkDescriptorSetLayout m_shadowDescriptorSetLayout;
	std::vector<VkDescriptorSet> m_shadowDescriptorSets;
	VkPipeline m_pipelineShadow;

	// CASCADE SHADOW
	std::vector<Cascade>		m_cascades; 

	std::vector<char> LoadingShader(const std::string& filename);
	void CreateGraphiquePipeline(const VulkanDeviceContext& context, const VulkanRenderContext& renderContext);
	VkShaderModule CreateShaderModule(const std::vector<char>& shader, const VulkanDeviceContext& context);
	void Clean(const VulkanDeviceContext& context);

};


struct VulkanImGui
{
	void Init(const VulkanRenderContext& rendercontext, const VulkanDeviceContext& context, const VulkanGraphiquePipeline graphique, GLFWwindow* window)
	{
		// Setup Dear ImGui rendercontext
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		
		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		//ImGui::StyleColorsClassic();

		// Setup Platform/Renderer bindings
		ImGui_ImplGlfw_InitForVulkan(window, true);
		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = context.m_instance;
		init_info.PhysicalDevice = context.m_physicalDevice;
		init_info.Device = context.m_device;
		init_info.QueueFamily = rendercontext.m_graphicsQueueIndex;
		init_info.Queue = rendercontext.m_graphicsQueue;
		init_info.DescriptorPool = graphique.m_descriptorPool;
		init_info.MinImageCount = context.SWAPCHAIN_IMAGES;
		init_info.ImageCount = rendercontext.PENDING_FRAMES;
		ImGui_ImplVulkan_Init(&init_info, rendercontext.m_renderPass);
	}

	void InitalizeFont(const VulkanRenderContext rendercontext, const VulkanDeviceContext& context)
	{
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(rendercontext.m_mainCommandBuffers[0], &beginInfo);

		ImGui_ImplVulkan_CreateFontsTexture(rendercontext.m_mainCommandBuffers[0]);

		VkSubmitInfo end_info = {};
		end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		end_info.commandBufferCount = 1;
		end_info.pCommandBuffers = &rendercontext.m_mainCommandBuffers[0];
		
		vkEndCommandBuffer(rendercontext.m_mainCommandBuffers[0]);
		DEBUG_CHECK_VK(vkQueueSubmit(rendercontext.m_graphicsQueue, 1, &end_info, VK_NULL_HANDLE));

		DEBUG_CHECK_VK(vkDeviceWaitIdle(context.m_device));
		ImGui_ImplVulkan_DestroyFontUploadObjects();

	}

	void DrawFrame(const VulkanRenderContext& rendercontext, const VulkanDeviceContext& context, VkCommandBuffer commandBuffer)
	{
		// Start the Dear ImGui frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// 3. Show another simple window.
		ImGui::Begin("Another Window");   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
		ImGui::Text("Hello from another window!");
//		if (ImGui::Button("Close Me"))
	//		m_show_another_window = false;
		ImGui::End();

		// Rendering
		ImGui::Render();
		
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
	}

	void Clean(const VulkanDeviceContext context)
	{
		vkDeviceWaitIdle(context.m_device);
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}
};

struct VulkanGraphicsApplication
{
	VulkanDeviceContext m_context;
	VulkanRenderContext m_rendercontext;
	VulkanGraphiquePipeline m_graphiquePipeline;
	VulkanGraphiquePipeline m_shadowPipeline;
	GLFWwindow* m_window;

	uint32_t m_frame;
	uint32_t m_currentFrame;

	VulkanImGui m_imgui;

	bool Initialize();
	bool Run();
	bool Display();
	bool Shutdown();
};

static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanReportFunc(
	VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT objType,
	uint64_t obj,
	size_t location,
	int32_t code,
	const char* layerPrefix,
	const char* msg,
	void* userData)
{
	std::cout << "[VULKAN VALIDATION]" << msg << std::endl;
#ifdef _WIN32
	if (IsDebuggerPresent()) {
		OutputDebugStringA("[VULKAN VALIDATION] ");
		OutputDebugStringA(msg);
		OutputDebugStringA("\n");
	}
#endif
	return VK_FALSE;
}

void GenerateQuadStrip(Mesh& mesh)
{
	mesh.m_indices.reserve(4);
	mesh.m_vertices.reserve(4);
		 
	mesh.m_vertices.push_back({ { -1.f, +1.f, 0.f },{ 0.f, 0.f },{ 0.f, 0.f, 1.f } });
	mesh.m_vertices.push_back({ { -1.f, -1.f, 0.f },{ 0.f, 0.f },{ 0.f, 0.f, 1.f } });
	mesh.m_vertices.push_back({ { +1.f, +1.f, 0.f },{ 0.f, 0.f },{ 0.f, 0.f, 1.f } });
	mesh.m_vertices.push_back({ { +1.f, -1.f, 0.f },{ 0.f, 0.f },{ 0.f, 0.f, 1.f } });
	mesh.m_indices.push_back(0);
	mesh.m_indices.push_back(1);
	mesh.m_indices.push_back(2);
	mesh.m_indices.push_back(3);
}

// 
// Sphere procedurale par revolution avec une typologie TRIANGLE STRIP
// le winding est CLOCKWISE (il faut inverser l'ordre des indices pour changer le winding)
//
void GenerateSphere(Mesh& mesh, int horizSegments, int vertiSegments, float sphereScale = 1.f)
{
	const float PI = 3.14159265359f;

	mesh.m_indices.reserve((vertiSegments + 1)*(horizSegments + 1));
	mesh.m_vertices.reserve((vertiSegments + 1)*(horizSegments + 1));

	for (unsigned int y = 0; y <= vertiSegments; ++y)
	{
		for (unsigned int x = 0; x <= horizSegments; ++x)
		{
			float xSegment = (float)x / (float)horizSegments;
			float ySegment = (float)y / (float)vertiSegments;
			float theta = ySegment * PI;
			float phi = xSegment * 2.0f * PI;
			float xPos = std::cos(phi) * std::sin(theta);
			float yPos = std::cos(theta);
			float zPos = std::sin(phi) * std::sin(theta);
			Vertex vtx{
				glm::vec3(xPos*sphereScale, yPos*sphereScale, zPos*sphereScale),
				glm::vec2(xSegment, ySegment),
				glm::vec3(xPos, yPos, zPos),
			};
			mesh.m_vertices.push_back(vtx);
		}
	}

	bool oddRow = false;
	for (int y = 0; y < vertiSegments; ++y)
	{
		if (!oddRow) // even rows: y == 0, y == 2; and so on
		{
			for (int x = 0; x <= horizSegments; ++x)
			{
				mesh.m_indices.push_back((y + 1) * (horizSegments + 1) + x);
				mesh.m_indices.push_back(y       * (horizSegments + 1) + x);
			}
		}
		else
		{
			for (int x = horizSegments; x >= 0; --x)
			{
				mesh.m_indices.push_back(y       * (horizSegments + 1) + x);
				mesh.m_indices.push_back((y + 1) * (horizSegments + 1) + x);
			}
		}
		oddRow = !oddRow;
	}
}


bool VulkanGraphicsApplication::Initialize()
{
	m_graphiquePipeline = VulkanGraphiquePipeline();


	// Vulkan

	DEBUG_CHECK_VK(volkInitialize());

	// instance

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "00_minimal";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "todo engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	const char *extensionNames[] = { VK_KHR_SURFACE_EXTENSION_NAME
#if defined(_WIN32)
		, VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif
		, VK_EXT_DEBUG_REPORT_EXTENSION_NAME };
	VkInstanceCreateInfo instanceInfo = {};
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pApplicationInfo = &appInfo;
	instanceInfo.enabledExtensionCount = sizeof(extensionNames) / sizeof(char*);
	instanceInfo.ppEnabledExtensionNames = extensionNames;
#ifdef VULKAN_ENABLE_VALIDATION
	const char* layerNames[] = { "VK_LAYER_LUNARG_standard_validation" };
	instanceInfo.enabledLayerCount = 1;
	instanceInfo.ppEnabledLayerNames = layerNames;
#else
	instanceInfo.enabledExtensionCount--;
#endif
	DEBUG_CHECK_VK(vkCreateInstance(&instanceInfo, nullptr, &m_context.m_instance));
	// TODO: fallback si pas de validation possible (MoltenVK, toujours le cas ?)

	volkLoadInstance(m_context.m_instance);

#ifdef VULKAN_ENABLE_VALIDATION
	VkDebugReportCallbackCreateInfoEXT debugCallbackInfo = {};
	debugCallbackInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	debugCallbackInfo.flags = VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT |
		VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT | VK_DEBUG_REPORT_INFORMATION_BIT_EXT;
	debugCallbackInfo.pfnCallback = VulkanReportFunc;
	vkCreateDebugReportCallbackEXT(m_context.m_instance, &debugCallbackInfo, nullptr, &m_context.m_debugCallback);
#endif

	// render surface

#if defined(USE_GLFW_SURFACE)
	glfwCreateWindowSurface(g_Context.instance, g_Context.window, nullptr, &g_Context.surface);
#else
#if defined(_WIN32)
	VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
	surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfaceInfo.hinstance = GetModuleHandle(NULL);
	surfaceInfo.hwnd = glfwGetWin32Window(m_window);
	DEBUG_CHECK_VK(vkCreateWin32SurfaceKHR(m_context.m_instance, &surfaceInfo, nullptr, &m_context.m_surface));
#endif
#endif

	// Imgui
	//ImGui_ImplVulkanH_Window* m_window = &m_window;

	// device

	uint32_t num_devices = m_context.MAX_DEVICE_COUNT;
	std::vector<VkPhysicalDevice> physical_devices(num_devices);
	DEBUG_CHECK_VK(vkEnumeratePhysicalDevices(m_context.m_instance, &num_devices, &physical_devices[0]));

	m_context.m_physicalDevice = physical_devices[0];

	// enumeration des memory types
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(m_context.m_physicalDevice, &memoryProperties);
	m_context.memoryFlags.reserve(memoryProperties.memoryTypeCount);
	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
		m_context.memoryFlags.push_back(memoryProperties.memoryTypes[i].propertyFlags);

	m_rendercontext.m_graphicsQueueIndex = UINT32_MAX;
	uint32_t queue_families_count = m_context.MAX_FAMILY_COUNT;
	std::vector<VkQueueFamilyProperties> queue_family_properties(queue_families_count);
	// normalement il faut appeler la fonction une premiere fois pour recuperer le nombre exact de queues supportees.
	//voir les messages de validation
	vkGetPhysicalDeviceQueueFamilyProperties(m_context.m_physicalDevice, &queue_families_count, &queue_family_properties[0]);
	for (uint32_t i = 0; i < queue_families_count; ++i) {
		if ((queue_family_properties[i].queueCount > 0) &&
			(queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
			VkBool32 canPresentSurface;
			vkGetPhysicalDeviceSurfaceSupportKHR(m_context.m_physicalDevice, i, m_context.m_surface, &canPresentSurface);
			if (canPresentSurface)
				m_rendercontext.m_graphicsQueueIndex = i;
			break;
		}
	}

	// on suppose que la presentation se fait par la graphics queue (verifier cela avec vkGetPhysicalDeviceSurfaceSupportKHR())
	m_rendercontext.m_presentQueueIndex = m_rendercontext.m_graphicsQueueIndex;

	const float queue_priorities[] = { 1.0f };
	VkDeviceQueueCreateInfo queueCreateInfo = {};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = m_rendercontext.m_graphicsQueueIndex;
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = queue_priorities;

	const char* device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	VkDeviceCreateInfo deviceInfo = {};
	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.queueCreateInfoCount = 1;
	deviceInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceInfo.enabledExtensionCount = 1;
	deviceInfo.ppEnabledExtensionNames = device_extensions;
	DEBUG_CHECK_VK(vkCreateDevice(m_context.m_physicalDevice, &deviceInfo, nullptr, &m_context.m_device));

	volkLoadDevice(m_context.m_device);

	vkGetDeviceQueue(m_context.m_device, m_rendercontext.m_graphicsQueueIndex, 0, &m_rendercontext.m_graphicsQueue);
	m_rendercontext.m_presentQueue = m_rendercontext.m_graphicsQueue;

	// swap chain

	// todo: enumerer (cf validation)
	uint32_t formatCount = 10;
	VkSurfaceFormatKHR surfaceFormats[10];
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_context.m_physicalDevice, m_context.m_surface, &formatCount, surfaceFormats);

	// on souhaite absolument avoir un format sRGB
	// donc conversion automatique en sortie du shader lineare vers gamma
	// attention ce n'est pas valable que pour la swapchain ici
	for (int i = 0; i < formatCount; i++)
	{
		if (surfaceFormats[i].format == VK_FORMAT_R8G8B8A8_SRGB || surfaceFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB)
		{
			m_context.m_surfaceFormat = surfaceFormats[i];
			break;
		}
	}

	if (m_context.m_surfaceFormat.format == VK_FORMAT_UNDEFINED)
		__debugbreak();

	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;   // FIFO garanti.
	// VK_PRESENT_MODE_IMMEDIATE_KHR

	uint32_t swapchainImageCount = m_context.SWAPCHAIN_IMAGES;

	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_context.m_physicalDevice, m_context.m_surface, &surfaceCapabilities);
	m_context.m_swapchainExtent = surfaceCapabilities.currentExtent;
	VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // garanti
	if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT; // necessaire ici pour vkCmdClearImageColor
//	if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
//		imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT; // necessaire ici pour screenshots, read back

	VkSwapchainCreateInfoKHR swapchainInfo = {};
	swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainInfo.surface = m_context.m_surface;
	swapchainInfo.minImageCount = swapchainImageCount;
	swapchainInfo.imageFormat = m_context.m_surfaceFormat.format;
	swapchainInfo.imageColorSpace = m_context.m_surfaceFormat.colorSpace;
	swapchainInfo.imageExtent = m_context.m_swapchainExtent;
	swapchainInfo.imageArrayLayers = 1; // 2 for stereo
	swapchainInfo.imageUsage = imageUsage;
	swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainInfo.preTransform = surfaceCapabilities.currentTransform;
	swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainInfo.presentMode = presentMode;
	swapchainInfo.clipped = VK_TRUE;
	DEBUG_CHECK_VK(vkCreateSwapchainKHR(m_context.m_device, &swapchainInfo, nullptr, &m_context.m_swapchain));

	vkGetSwapchainImagesKHR(m_context.m_device, m_context.m_swapchain, &swapchainImageCount, m_context.m_swapchainImages);


	//VkImageSubresourceRange subRange;
	m_rendercontext.m_mainSubRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	m_rendercontext.m_mainSubRange.baseMipLevel = 0;
	m_rendercontext.m_mainSubRange.levelCount = 1;
	m_rendercontext.m_mainSubRange.baseArrayLayer = 0;
	m_rendercontext.m_mainSubRange.layerCount = 1;

	VkImageViewCreateInfo imageViewCreateInfo = {};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY };
	imageViewCreateInfo.format = m_context.m_surfaceFormat.format;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.subresourceRange = m_rendercontext.m_mainSubRange;

	for (int imageViewIdx = 0; imageViewIdx < m_context.SWAPCHAIN_IMAGES; imageViewIdx++)
	{
		imageViewCreateInfo.image = m_context.m_swapchainImages[imageViewIdx];
		vkCreateImageView(m_context.m_device, &imageViewCreateInfo, nullptr, &m_context.m_swapChainImageViews[imageViewIdx]);
	}

	// Command buffers

	VkCommandPoolCreateInfo commandInfo = {};
	commandInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandInfo.queueFamilyIndex = m_rendercontext.m_graphicsQueueIndex;
	vkCreateCommandPool(m_context.m_device, &commandInfo, nullptr, &m_rendercontext.m_mainCommandPool);

	VkCommandBufferAllocateInfo commandAllocInfo = {};
	commandAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandAllocInfo.commandPool = m_rendercontext.m_mainCommandPool;
	commandAllocInfo.commandBufferCount = 2;
	commandAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	vkAllocateCommandBuffers(m_context.m_device, &commandAllocInfo, m_rendercontext.m_mainCommandBuffers);


	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // SINON ON BLOQUE SUR LE WAITFORFENCE

	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	for (int i = 0; i < m_rendercontext.PENDING_FRAMES; i++)
		vkCreateFence(m_context.m_device, &fenceCreateInfo, nullptr, &m_rendercontext.m_mainFences[i]);

	for (int j = 0; j < m_context.SWAPCHAIN_IMAGES; j++)
	{
		vkCreateSemaphore(m_context.m_device, &semaphoreCreateInfo, nullptr, &m_context.m_aquireSemaphores[j]);
		vkCreateSemaphore(m_context.m_device, &semaphoreCreateInfo, nullptr, &m_context.m_presentSemaphores[j]);
	}

	// 1. Create renderPass
	//	1.a definir les attachments (VKattachmentDespcription, un tableau)
	//		VkAttachmentDescription permet de specifier le format mais surtout les action a effectuer lorsque 
	//			l'attachment est load par la subpass (loadOp, peut etre DONT_CARE)
	//			l'attachment est store par la subpass (storeOp, peut etre DONT_CARE)
	//			les layout initial (src layout) et final  (dst layout)
	//	1.b definir les subpasses (VKSubpassDescription, tableau )
	//		VkSubpassDescription indique les attachments pour cette subpass
	//			pour eviter de tout redefinir on passe par un VkAttachmentReference
	//			cette reference est ici l'INDEX de l'attachment dans le tableau de VkAttachmentDescription
	// 2. Create framebuffer
	//		Attention, 1 frame buffer par image dans la swapchain ici

	// Depth Buffer
	RenderSurface& depthBuffer = m_rendercontext.m_depthBuffer;
	depthBuffer.m_format = VK_FORMAT_D32_SFLOAT;
	{
		VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT; //LAZILY_ALLOCATED_BIT;
		VkExtent3D extent = { m_context.m_swapchainExtent.width, m_context.m_swapchainExtent.height, 1 };
		m_context.CreateImage(m_rendercontext.m_depthBuffer.m_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, properties, extent, 1, depthBuffer.m_image, depthBuffer.m_memory);

		VkImageViewCreateInfo viewInfo = {};
		{
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = depthBuffer.m_image;
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = depthBuffer.m_format;
			m_rendercontext.m_mainSubRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			viewInfo.subresourceRange = m_rendercontext.m_mainSubRange;
		}

		DEBUG_CHECK_VK(vkCreateImageView(m_context.m_device, &viewInfo, nullptr, &depthBuffer.m_imageView));


	}

	// stock les images (geometrie shader)
	enum RenderTarget
	{
		COLOR = 0,
		DEPTH = 1,
		MAX
	};

	VkAttachmentDescription attachDesc[RenderTarget::MAX] = {};
	{
		int id = RenderTarget::COLOR;
		attachDesc[id].format = m_context.m_surfaceFormat.format;
		attachDesc[id].samples = VK_SAMPLE_COUNT_1_BIT;
		attachDesc[id].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachDesc[id].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachDesc[id].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachDesc[id].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachDesc[id].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachDesc[id].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		id = RenderTarget::DEPTH;
		attachDesc[id].format = m_rendercontext.m_depthBuffer.m_format;
		attachDesc[id].samples = VK_SAMPLE_COUNT_1_BIT;
		attachDesc[id].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachDesc[id].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachDesc[id].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachDesc[id].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachDesc[id].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachDesc[id].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	};

	VkAttachmentReference attachmentReference[RenderTarget::MAX] = {};
	{
		int id = RenderTarget::COLOR;
		attachmentReference[id].attachment = RenderTarget::COLOR;
		attachmentReference[id].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		id = RenderTarget::DEPTH;
		attachmentReference[id].attachment = RenderTarget::DEPTH;
		attachmentReference[id].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}

	VkSubpassDescription subpassDescription = {};
	{
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = attachmentReference;
		subpassDescription.pDepthStencilAttachment = &attachmentReference[1];
	}

	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.pAttachments = attachDesc;
	renderPassCreateInfo.attachmentCount = 2;
	renderPassCreateInfo.pSubpasses = &subpassDescription;
	renderPassCreateInfo.subpassCount = 1;

	vkCreateRenderPass(m_context.m_device, &renderPassCreateInfo, nullptr, &m_rendercontext.m_renderPass);

	VkFramebufferCreateInfo frameBufferCreateInfo = {};
	frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frameBufferCreateInfo.renderPass = m_rendercontext.m_renderPass;
	frameBufferCreateInfo.layers = 1;
	frameBufferCreateInfo.attachmentCount = 2;
	frameBufferCreateInfo.height = 600;
	frameBufferCreateInfo.width = 800;

	VkImageView frameBufferAttachments[2] = { nullptr,m_rendercontext.m_depthBuffer.m_imageView };
	for (int frameBufferIdx = 0; frameBufferIdx < m_rendercontext.PENDING_FRAMES; ++frameBufferIdx)
	{
		frameBufferAttachments[0] = m_context.m_swapChainImageViews[frameBufferIdx];
		frameBufferCreateInfo.pAttachments = frameBufferAttachments;
		vkCreateFramebuffer(m_context.m_device, &frameBufferCreateInfo, nullptr, &m_rendercontext.m_frameBuffers[frameBufferIdx]);
	}


	m_graphiquePipeline.m_scene.m_meshes.resize(5);

	GenerateSphere(m_graphiquePipeline.m_scene.m_meshes[0], 64, 64, 1.0f);


	/// Texture Mapping
	Texture& textureSphere = m_graphiquePipeline.m_scene.m_meshes[0].m_texture;
	textureSphere.CreateTextureColor(m_rendercontext, m_context);
	textureSphere.CreateTextureImageView(m_context);
	textureSphere.CreateTextureSampler(m_context);
	///
	
	GenerateSphere(m_graphiquePipeline.m_scene.m_meshes[1], 64, 64, 1.0f);
	//GenerateQuadStrip(m_graphiquePipeline.m_scene.m_meshes[1]);
	/*Texture& textureCube = m_graphiquePipeline.m_scene.m_meshes[meshIdx].m_texture;
	textureCube.CreateTextureColor(m_rendercontext, m_context);
	textureCube.CreateTextureImageView(m_context);
	textureCube.CreateTextureSampler(m_context);
*/

	GenerateSphere(m_graphiquePipeline.m_scene.m_meshes[2], 64, 64, 1.0f);
	GenerateSphere(m_graphiquePipeline.m_scene.m_meshes[3], 64, 64, 1.0f);
	GenerateSphere(m_graphiquePipeline.m_scene.m_meshes[4], 64, 64, 1.0f);

	m_graphiquePipeline.CreateGraphiquePipeline(m_context, m_rendercontext);

	Buffer::CreateUniformBuffers(m_context, sizeof(VP), 1, m_rendercontext.m_uniformBuffers, m_rendercontext.m_uniformBuffersMemory); //
	Buffer::CreateUniformBuffers(m_context, sizeof(Model), 1000, m_rendercontext.m_uniformBuffersDynamic, m_rendercontext.m_uniformBuffersMemoryDynamic); //Dynamic
	
	Mesh& sphereMesh = m_graphiquePipeline.m_scene.m_meshes[0];
	sphereMesh.m_buffer.CreateVertexBuffer(m_context, sphereMesh.m_vertices);
	sphereMesh.m_buffer.CreateIndexBuffer(m_context, sphereMesh.m_indices);
	sphereMesh.m_model.model = glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 0.0f, 4.0f));

	Mesh& cubeMesh = m_graphiquePipeline.m_scene.m_meshes[1];
	cubeMesh.m_buffer.CreateVertexBuffer(m_context, sphereMesh.m_vertices);
	cubeMesh.m_buffer.CreateIndexBuffer(m_context, sphereMesh.m_indices);
	cubeMesh.m_model.model = glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 0.0f, -10.0f));
	//cubeMesh.m_model.model = glm::scale(cubeMesh.m_model.model, glm::vec3(4.0f, 4.f, 4.0f));

	Mesh& sphereMesh3 = m_graphiquePipeline.m_scene.m_meshes[2];
	sphereMesh3.m_buffer.CreateVertexBuffer(m_context, sphereMesh3.m_vertices);
	sphereMesh3.m_buffer.CreateIndexBuffer(m_context, sphereMesh3.m_indices);
	sphereMesh3.m_model.model = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, -20.0f));
	//sphereMesh3.m_model.model = glm::scale(sphereMesh3.m_model.model, glm::vec3(4.0f, 4.0f, 4.f));

	Mesh& sphereMesh6 = m_graphiquePipeline.m_scene.m_meshes[3];
	sphereMesh6.m_buffer.CreateVertexBuffer(m_context, sphereMesh6.m_vertices);
	sphereMesh6.m_buffer.CreateIndexBuffer(m_context, sphereMesh6.m_indices);
	sphereMesh6.m_model.model = glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, 0.0f, 9.0f));
	sphereMesh6.m_model.model = glm::scale(sphereMesh6.m_model.model, glm::vec3(0.2f, 0.2f, 0.2f));

	Mesh& sphereMesh8 = m_graphiquePipeline.m_scene.m_meshes[4];
	sphereMesh8.m_buffer.CreateVertexBuffer(m_context, sphereMesh8.m_vertices);
	sphereMesh8.m_buffer.CreateIndexBuffer(m_context, sphereMesh8.m_indices);
	sphereMesh8.m_model.model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -25.0f));
	sphereMesh8.m_model.model = glm::scale(sphereMesh8.m_model.model, glm::vec3(100.0f, 100.0f, 0.1f));
	/*
	VkDescriptorPoolSize poolSize[2] = {};
	{
		poolSize[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSize[0].descriptorCount = static_cast<uint32_t>(m_context.SWAPCHAIN_IMAGES);
		poolSize[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSize[1].descriptorCount = 1;
	}*/

	VkDescriptorPoolSize poolSizes[12] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(m_context.SWAPCHAIN_IMAGES * 2)}
		
	};

	VkDescriptorPoolCreateInfo poolInfo = {};
	{
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 12;
		poolInfo.pPoolSizes = poolSizes;
		poolInfo.maxSets = 11002;
	}

	DEBUG_CHECK_VK(vkCreateDescriptorPool(m_context.m_device, &poolInfo, nullptr, &m_graphiquePipeline.m_descriptorPool));

	m_imgui.Init(m_rendercontext, m_context, m_graphiquePipeline, m_window);
	m_imgui.InitalizeFont(m_rendercontext, m_context);


	std::vector<VkDescriptorSetLayout> layouts(m_context.SWAPCHAIN_IMAGES, m_graphiquePipeline.m_descriptorSetLayout);

	VkDescriptorSetAllocateInfo allocInfo = {};
	{
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_graphiquePipeline.m_descriptorPool;
		allocInfo.descriptorSetCount = static_cast<uint32_t>(m_context.SWAPCHAIN_IMAGES);
		allocInfo.pSetLayouts = layouts.data();
	}

	m_graphiquePipeline.m_descriptorSets.resize(m_context.SWAPCHAIN_IMAGES);
	DEBUG_CHECK_VK(vkAllocateDescriptorSets(m_context.m_device, &allocInfo, m_graphiquePipeline.m_descriptorSets.data()));

	VkFormat depthFormat = m_context.FindDepthFormat();
	
	////////////////// SHADOW MAPPING //////////////////////////

	RenderSurface& shadowBuffer = m_rendercontext.m_shadowBuffer;
	shadowBuffer.m_format = VK_FORMAT_D32_SFLOAT;
	{
		// --------------------------------------------------------------------------------

		// SHADOW RENDERPASS
		VkAttachmentDescription shadowAttachment = {};
		{
			shadowAttachment.format = VK_FORMAT_D32_SFLOAT;
			shadowAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			shadowAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			shadowAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			shadowAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			shadowAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			shadowAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			shadowAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}

		VkAttachmentReference shadowAttRef = {};
		{
			shadowAttRef.attachment = 0;
			shadowAttRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		VkSubpassDescription shadowSubpass = {};
		{
			shadowSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			shadowSubpass.flags = 0;
			shadowSubpass.pDepthStencilAttachment = &shadowAttRef;
		}

		VkRenderPassCreateInfo shadowRenderPassInfo = {};
		{
			shadowRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			shadowRenderPassInfo.attachmentCount = 1;
			shadowRenderPassInfo.pAttachments = &shadowAttachment;
			shadowRenderPassInfo.subpassCount = 1;
			shadowRenderPassInfo.pSubpasses = &shadowSubpass;
		}

		DEBUG_CHECK_VK(vkCreateRenderPass(m_context.m_device, &shadowRenderPassInfo, nullptr, &m_rendercontext.m_shadowRenderPass));

		// --------------------------------------------------------------------------------

		// SHADOW IMAGE 
		VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT; //LAZILY_ALLOCATED_BIT;
		m_context.m_shadowExtent = { SHADOWMAP_DIM, SHADOWMAP_DIM, 1 };
		m_context.CreateImage(m_rendercontext.m_shadowBuffer.m_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, properties, m_context.m_shadowExtent, SHADOWMAP_CASCADE, shadowBuffer.m_image, shadowBuffer.m_memory);

		VkImageViewCreateInfo shadowViewInfo = {};
		{
			shadowViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			shadowViewInfo.image = shadowBuffer.m_image;
			shadowViewInfo.format = VK_FORMAT_D32_SFLOAT;
			shadowViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
			shadowViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
			shadowViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
			shadowViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
			shadowViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			shadowViewInfo.subresourceRange.baseMipLevel = 0;
			shadowViewInfo.subresourceRange.levelCount = 1;
			shadowViewInfo.subresourceRange.baseArrayLayer = 0;
			shadowViewInfo.subresourceRange.layerCount = SHADOWMAP_CASCADE;
			shadowViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			shadowViewInfo.flags = 0;
		}

		DEBUG_CHECK_VK(vkCreateImageView(m_context.m_device, &shadowViewInfo, nullptr, &shadowBuffer.m_imageView));

		// -----------------------------------------------------------------------------------

		
		// nb of cascade (4 for now)
		m_graphiquePipeline.m_cascades.resize(SHADOWMAP_CASCADE);
		
		// One image and frambuffer per cascade
		for (uint32_t idxCascade = 0; idxCascade < SHADOWMAP_CASCADE; idxCascade++)
		{
			// ImageView -> inside shadowMap
			VkImageViewCreateInfo cascadeViewInfo = {};
			{
				cascadeViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				cascadeViewInfo.image = shadowBuffer.m_image;
				cascadeViewInfo.format = shadowBuffer.m_format;
				cascadeViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
				cascadeViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
				cascadeViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
				cascadeViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
				cascadeViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
				cascadeViewInfo.subresourceRange.baseMipLevel = 0;
				cascadeViewInfo.subresourceRange.levelCount = 1;
				cascadeViewInfo.subresourceRange.baseArrayLayer = idxCascade;
				cascadeViewInfo.subresourceRange.layerCount = 1;
				cascadeViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
				cascadeViewInfo.flags = 0;
			}

			DEBUG_CHECK_VK(vkCreateImageView(m_context.m_device, &cascadeViewInfo, nullptr, &m_graphiquePipeline.m_cascades[idxCascade].m_imageView));

			VkFramebufferCreateInfo cascadeFramebufferInfo = {};
			{
				cascadeFramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				cascadeFramebufferInfo.renderPass = m_rendercontext.m_shadowRenderPass;
				cascadeFramebufferInfo.attachmentCount = 1;
				cascadeFramebufferInfo.pAttachments = &m_graphiquePipeline.m_cascades[idxCascade].m_imageView;
				cascadeFramebufferInfo.width = m_context.m_shadowExtent.width;
				cascadeFramebufferInfo.height = m_context.m_shadowExtent.height;
				cascadeFramebufferInfo.layers = 1;
			}

			DEBUG_CHECK_VK(vkCreateFramebuffer(m_context.m_device, &cascadeFramebufferInfo, nullptr, &m_graphiquePipeline.m_cascades[idxCascade].m_frameBuffer));
		}
		
		// -----------------------------------------------------------------------------------

		VkSamplerCreateInfo shadowSampler = {};
		{
			// SHADOW TODO
			shadowSampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			shadowSampler.magFilter = VK_FILTER_LINEAR; 
			shadowSampler.minFilter = VK_FILTER_LINEAR;
			shadowSampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			shadowSampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			shadowSampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			shadowSampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			shadowSampler.compareEnable = false;
			shadowSampler.compareOp = VK_COMPARE_OP_ALWAYS;
			shadowSampler.mipLodBias = 0.0f;
			shadowSampler.maxAnisotropy = 1.0f;
			shadowSampler.minLod = 0.0f;
			shadowSampler.maxLod = 100.0f;
			shadowSampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		}

		DEBUG_CHECK_VK(vkCreateSampler(m_context.m_device, &shadowSampler, nullptr, &shadowBuffer.m_shadowSampler));

		////////////////////////////// PIPELINES SHADOW ////////////////////////////////

		std::vector<char> vertexShader = m_graphiquePipeline.LoadingShader("Shaders/Shadow.vert.spv");

		VkShaderModule vertShaderModule = m_graphiquePipeline.CreateShaderModule(vertexShader, m_context);

		VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		VkVertexInputBindingDescription bindingDescription = Vertex::GetBindingDescription();
		std::array<VkVertexInputAttributeDescription, 3>& attributeDescription = Vertex::GetAttributeDescriptions();

		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
		{
			vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			vertexInputInfo.vertexBindingDescriptionCount = 1;
			vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
			vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size());
			vertexInputInfo.pVertexAttributeDescriptions = attributeDescription.data();
		}

		VkPipelineShaderStageCreateInfo shadowShaderStage[] = { vertShaderStageInfo };
		
		VkViewport viewport = {};
		{
			viewport.height = m_context.m_shadowExtent.height;
			viewport.width = m_context.m_shadowExtent.width;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			viewport.x = 0;
			viewport.y = 0;
		}

		VkRect2D scissor = {};
		{
			scissor.extent.width = m_context.m_shadowExtent.width;
			scissor.extent.height = m_context.m_shadowExtent.height;
			scissor.offset.x = 0;
			scissor.offset.y = 0;
		}

		VkPipelineViewportStateCreateInfo viewportState = {};
		{
			viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportState.viewportCount = 1;
			viewportState.pViewports = &viewport;
			viewportState.scissorCount = 1;
			viewportState.pScissors = &scissor;
		}

		VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
		{
			inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
			inputAssembly.primitiveRestartEnable = VK_FALSE;
		}

		VkPipelineRasterizationStateCreateInfo rasterizer = {};
		{
			rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
			rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
			rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
			rasterizer.lineWidth = 1.0f;
		}

		VkPipelineDepthStencilStateCreateInfo depthStencil = {};
		{
			depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			depthStencil.depthTestEnable = VK_TRUE;
			depthStencil.depthWriteEnable = VK_TRUE;
			depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
			depthStencil.depthBoundsTestEnable = VK_FALSE;

		}

		VkPipelineMultisampleStateCreateInfo multisampling = {};
		{
			multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		}

		VkDynamicState dynamicStates[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_LINE_WIDTH
		};

		VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
		{
			dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			dynamicStateCreateInfo.dynamicStateCount = 2;
			dynamicStateCreateInfo.pDynamicStates = dynamicStates;
		}

		VkPushConstantRange constantRange = { VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) };
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
		{
			pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutInfo.pushConstantRangeCount = 1;
			pipelineLayoutInfo.pPushConstantRanges = &constantRange;
			pipelineLayoutInfo.setLayoutCount = 1;
			pipelineLayoutInfo.pSetLayouts = &m_graphiquePipeline.m_descriptorSetLayout;
		}

		if (vkCreatePipelineLayout(m_context.m_device, &pipelineLayoutInfo, nullptr, &m_graphiquePipeline.m_shadowPipelineLayout) != VK_SUCCESS)
			throw std::runtime_error("Failed to create pipeline layout!");

		m_graphiquePipeline.m_shadowPipelineInfo = {};
		{
			m_graphiquePipeline.m_shadowPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			m_graphiquePipeline.m_shadowPipelineInfo.pViewportState = &viewportState;
			m_graphiquePipeline.m_shadowPipelineInfo.pVertexInputState = &vertexInputInfo;
			m_graphiquePipeline.m_shadowPipelineInfo.pInputAssemblyState = &inputAssembly;
			m_graphiquePipeline.m_shadowPipelineInfo.pRasterizationState = &rasterizer;
			m_graphiquePipeline.m_shadowPipelineInfo.pMultisampleState = &multisampling;
			m_graphiquePipeline.m_shadowPipelineInfo.pDepthStencilState = &depthStencil;
								
			m_graphiquePipeline.m_shadowPipelineInfo.renderPass = m_rendercontext.m_shadowRenderPass;
								
			m_graphiquePipeline.m_shadowPipelineInfo.stageCount = 1;
			m_graphiquePipeline.m_shadowPipelineInfo.pStages = shadowShaderStage;
								
			m_graphiquePipeline.m_shadowPipelineInfo.layout = m_graphiquePipeline.m_shadowPipelineLayout;
								
			m_graphiquePipeline.m_shadowPipelineInfo.basePipelineIndex = -1;
			m_graphiquePipeline.m_shadowPipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
			m_graphiquePipeline.m_shadowPipelineInfo.subpass = 0;
		}

		///////////// UPDATE PIPELINE INFO FOR SHADOW ///////////////////////

		DEBUG_CHECK_VK(vkCreateGraphicsPipelines(m_context.m_device, nullptr, 1, &m_graphiquePipeline.m_shadowPipelineInfo, nullptr, &m_graphiquePipeline.m_pipelineShadow));

		vkDestroyShaderModule(m_context.m_device, vertShaderModule, nullptr);

		std::vector<VkDescriptorSetLayout> layoutShadow(m_context.SWAPCHAIN_IMAGES, m_graphiquePipeline.m_shadowDescriptorSetLayout);
		
		allocInfo.pSetLayouts = layoutShadow.data();
	}
	/////////////////////////// END SHADOW ////////////////////////////////////////


	for (size_t i = 0; i < m_context.SWAPCHAIN_IMAGES; i++)
	{
		VkDescriptorBufferInfo bufferInfo = {};
		{
			bufferInfo.buffer = m_rendercontext.m_uniformBuffers[i];
			bufferInfo.offset = 0;
			bufferInfo.range = sizeof(VP);
		}

		VkDescriptorBufferInfo bufferInfoDynamic = {};
		{
			bufferInfoDynamic.buffer = m_rendercontext.m_uniformBuffersDynamic[i];
			bufferInfoDynamic.offset = 0;
			bufferInfoDynamic.range = sizeof(Model);
		}

		VkDescriptorImageInfo imageInfo = {};
		{
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = textureSphere.m_imageView;
			imageInfo.sampler = textureSphere.m_sampler;
		}

		VkDescriptorImageInfo shadowImageInfo = {};
		{
			shadowImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			shadowImageInfo.imageView = shadowBuffer.m_imageView;
			shadowImageInfo.sampler = shadowBuffer.m_shadowSampler;
		}

		VkWriteDescriptorSet descriptorWrite[4] = {};
		{
			descriptorWrite[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite[0].dstSet = m_graphiquePipeline.m_descriptorSets[i];
			descriptorWrite[0].dstBinding = 0;
			descriptorWrite[0].dstArrayElement = 0;
			descriptorWrite[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorWrite[0].descriptorCount = 1;
			descriptorWrite[0].pBufferInfo = &bufferInfo;

			descriptorWrite[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite[1].dstSet = m_graphiquePipeline.m_descriptorSets[i];
			descriptorWrite[1].dstBinding = 1;
			descriptorWrite[1].dstArrayElement = 0;
			descriptorWrite[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrite[1].descriptorCount = 1;
			descriptorWrite[1].pImageInfo = &imageInfo;

			descriptorWrite[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite[2].dstSet = m_graphiquePipeline.m_descriptorSets[i];
			descriptorWrite[2].dstBinding = 2;
			descriptorWrite[2].dstArrayElement = 0;
			descriptorWrite[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC; // TODO -> dynamic
			descriptorWrite[2].descriptorCount = 1;
			descriptorWrite[2].pBufferInfo = &bufferInfoDynamic;

			// Shadow Sampler / same descriptor Set for Color and shadow
			descriptorWrite[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite[3].dstSet = m_graphiquePipeline.m_descriptorSets[i];
			descriptorWrite[3].dstBinding = 3;
			descriptorWrite[3].dstArrayElement = 0;
			descriptorWrite[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrite[3].descriptorCount = 1;
			descriptorWrite[3].pImageInfo = &shadowImageInfo;
		}

		vkUpdateDescriptorSets(m_context.m_device, 4, descriptorWrite, 0, nullptr);
	}

	m_frame = 0;
	m_currentFrame = 0;

	return true;
}

bool VulkanGraphicsApplication::Shutdown()
{
	if (m_context.m_instance == VK_NULL_HANDLE)
		return false;
	if (m_context.m_device == VK_NULL_HANDLE)
		return false;

	for (int i = 0; i < m_rendercontext.PENDING_FRAMES; i++)
		vkDestroyFence(m_context.m_device, m_rendercontext.m_mainFences[i], nullptr);
	
	for (int j = 0; j < m_context.SWAPCHAIN_IMAGES; j++)
	{
		vkDestroyFramebuffer(m_context.m_device, m_rendercontext.m_frameBuffers[j], nullptr);
		vkDestroyImageView(m_context.m_device, m_context.m_swapChainImageViews[j], nullptr);
		vkDestroySemaphore(m_context.m_device, m_context.m_aquireSemaphores[j], nullptr);
		vkDestroySemaphore(m_context.m_device, m_context.m_presentSemaphores[j], nullptr);
	}
	
	vkDestroyRenderPass(m_context.m_device, m_rendercontext.m_renderPass, nullptr);

	vkDeviceWaitIdle(m_context.m_device);

	vkFreeCommandBuffers(m_context.m_device, m_rendercontext.m_mainCommandPool, 1, m_rendercontext.m_mainCommandBuffers);

	vkDestroyCommandPool(m_context.m_device, m_rendercontext.m_mainCommandPool, nullptr);

	vkDestroySwapchainKHR(m_context.m_device, m_context.m_swapchain, nullptr);

	/*vkDestroyBuffer(m_context.m_device, m_rendercontext.m_indexBuffer, nullptr);
	vkFreeMemory(m_context.m_device, m_rendercontext.m_indexBufferMemory, nullptr);

	vkDestroyBuffer(m_context.m_device, m_rendercontext.m_vertexBuffer, nullptr);
	vkFreeMemory(m_context.m_device, m_rendercontext.m_vertexBufferMemory, nullptr);
	*/
	for (size_t i = 0; i < m_context.SWAPCHAIN_IMAGES; i++)
	{
		vkDestroyBuffer(m_context.m_device, m_rendercontext.m_uniformBuffers[i], nullptr);
		vkFreeMemory(m_context.m_device, m_rendercontext.m_uniformBuffersMemory[i], nullptr);
		
		vkDestroyBuffer(m_context.m_device, m_rendercontext.m_uniformBuffersDynamic[i], nullptr);
		vkFreeMemory(m_context.m_device, m_rendercontext.m_uniformBuffersMemoryDynamic[i], nullptr);
	}

	vkDestroyDevice(m_context.m_device, nullptr);

	vkDestroySurfaceKHR(m_context.m_instance, m_context.m_surface, nullptr);

#ifdef VULKAN_ENABLE_VALIDATION
	vkDestroyDebugReportCallbackEXT(m_context.m_instance, m_context.m_debugCallback, nullptr);
#endif

	vkDestroyInstance(m_context.m_instance, nullptr);

#ifndef GLFW_INCLUDE_VULKAN
	
#endif

	return true;
}



std::vector<char> VulkanGraphiquePipeline::LoadingShader(const std::string& filename)
{
	std::ifstream file (filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
		throw std::runtime_error("failed to open file!");

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();

	return buffer;
}

void VulkanGraphiquePipeline::CreateGraphiquePipeline(const VulkanDeviceContext& context, const VulkanRenderContext& renderContext)
{
	std::vector<char> vertexShader = LoadingShader("Shaders/VertexData.vert.spv");
	std::vector<char> fragmentShader = LoadingShader("Shaders/VertexData.frag.spv");

	VkShaderModule vertShaderModule = CreateShaderModule(vertexShader, context);
	VkShaderModule fragShaderModule = CreateShaderModule(fragmentShader, context);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStage[] = { vertShaderStageInfo , fragShaderStageInfo};

	VkVertexInputBindingDescription bindingDescription = Vertex::GetBindingDescription();
	std::array<VkVertexInputAttributeDescription, 3>& attributeDescription = Vertex::GetAttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	{
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription; 
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size());
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescription.data();
	}

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	{
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		inputAssembly.primitiveRestartEnable = VK_FALSE;
	}

	VkViewport viewport = {};
	{
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)context.m_swapchainExtent.width;
		viewport.height = (float)context.m_swapchainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
	}

	VkRect2D scissor = {
		{ 0, 0 },{ viewport.width, viewport.height }
	};

	VkPipelineViewportStateCreateInfo viewportState = {};
	{
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;
	}

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	{
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.lineWidth = 1.0f;
	}

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	{
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	/*	colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.blendEnable = VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;*/
	}

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	{
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
	}

	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	{
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;

	}

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	{
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	}

	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_LINE_WIDTH
	};

	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	{
		dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStateCreateInfo.dynamicStateCount = 2;
		dynamicStateCreateInfo.pDynamicStates = dynamicStates;
	}

	VkDescriptorSetLayoutBinding vpLayoutBinding = {};
	{
		vpLayoutBinding.binding = 0;
		vpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		vpLayoutBinding.descriptorCount = 1;
		vpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	}

	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	{
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	}

	VkDescriptorSetLayoutBinding modelLayoutBinding = {};
	{
		modelLayoutBinding.binding = 2;
		modelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		modelLayoutBinding.descriptorCount = 1;
		modelLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	}

	VkDescriptorSetLayoutBinding shadowSamplerLayoutBinding = {};
	{
		shadowSamplerLayoutBinding.binding = 3;
		shadowSamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		shadowSamplerLayoutBinding.descriptorCount = 1;
		shadowSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	}

	std::array<VkDescriptorSetLayoutBinding, 4> bindings = { vpLayoutBinding, samplerLayoutBinding, modelLayoutBinding, shadowSamplerLayoutBinding};

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	{
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data();
	}

	DEBUG_CHECK_VK(vkCreateDescriptorSetLayout(context.m_device, &layoutInfo, nullptr, &m_descriptorSetLayout));


	VkPushConstantRange constantRange = { VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) };
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	{
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &constantRange;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
	}

	if (vkCreatePipelineLayout(context.m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
		throw std::runtime_error("Failed to create pipeline layout!");

	m_pipelineInfo = {};
	{
		m_pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		m_pipelineInfo.pViewportState = &viewportState;
		m_pipelineInfo.pVertexInputState = &vertexInputInfo;
		m_pipelineInfo.pInputAssemblyState = &inputAssembly;
		m_pipelineInfo.pRasterizationState = &rasterizer;
		m_pipelineInfo.pMultisampleState = &multisampling;
		m_pipelineInfo.pColorBlendState = &colorBlending;
		m_pipelineInfo.pDepthStencilState = &depthStencil;

		m_pipelineInfo.renderPass = renderContext.m_renderPass;
		
		m_pipelineInfo.stageCount = 2;
		m_pipelineInfo.pStages = shaderStage;
		
		m_pipelineInfo.layout = m_pipelineLayout;

		m_pipelineInfo.basePipelineIndex = -1;
		m_pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		m_pipelineInfo.subpass = 0;
	}

	if (vkCreateGraphicsPipelines(context.m_device, VK_NULL_HANDLE, 1, &m_pipelineInfo, nullptr, &m_pipelineOpaque) != VK_SUCCESS)
		throw std::runtime_error("failed to create graphics pipeline!");

	vkDestroyShaderModule(context.m_device, fragShaderModule ,nullptr);
	vkDestroyShaderModule(context.m_device, vertShaderModule, nullptr);
}

VkShaderModule VulkanGraphiquePipeline::CreateShaderModule(const std::vector<char>& shader, const VulkanDeviceContext& context)
{
	VkShaderModuleCreateInfo shaderInfo = {};
	shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderInfo.codeSize = shader.size();
	shaderInfo.pCode = reinterpret_cast<const uint32_t*>(shader.data());

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(context.m_device, &shaderInfo, nullptr, &shaderModule) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create shader module!");
	}

	return shaderModule;
}

void VulkanGraphiquePipeline::Clean(const VulkanDeviceContext& context)
{
	vkDestroyDescriptorSetLayout(context.m_device, m_descriptorSetLayout, nullptr);
	vkDestroyPipeline(context.m_device, m_pipelineOpaque, nullptr);
	vkDestroyPipelineLayout(context.m_device, m_pipelineLayout, nullptr);
}

bool VulkanGraphicsApplication::Display()
{
	int width, height;
	glfwGetWindowSize(m_window, &width, &height);
	
	static double previousTime = glfwGetTime();
	static double currentTime = glfwGetTime();

	currentTime = glfwGetTime();
	double deltaTime = currentTime - previousTime;
	std::cout << "[" << m_frame << "] frame time = " << deltaTime*1000.0 << " ms [" << 1.0/deltaTime << " fps]" << std::endl;
	previousTime = currentTime;

	// RENDU ICI
	// RENAME COMMANDBUFFER
	VkCommandBuffer commandBuffer = m_rendercontext.m_mainCommandBuffers[m_currentFrame];
	// On bloque tant que la fence soumis avec un command buffer n'a pas ete signale
	//vkDeviceWaitIdle(context.device);
	vkWaitForFences(m_context.m_device,1, &m_rendercontext.m_mainFences[m_currentFrame], false, UINT64_MAX);
	vkResetFences(m_context.m_device,1, &m_rendercontext.m_mainFences[m_currentFrame]);
	vkResetCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

	VkSemaphore* aquireSem = &m_context.m_aquireSemaphores[m_currentFrame];
	VkSemaphore* presentSem = &m_context.m_presentSemaphores[m_currentFrame];


	uint32_t image_index;
	uint64_t timeout = UINT64_MAX;
	DEBUG_CHECK_VK(vkAcquireNextImageKHR(m_context.m_device, m_context.m_swapchain, timeout, *aquireSem/*acquire semaphore ici*/, VK_NULL_HANDLE, &image_index));


	VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(commandBuffer, &beginInfo);


#if 0
	VkImageSubresourceRange subRange;
		subRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subRange.baseMipLevel = 0;
		subRange.levelCount = 1;
		subRange.baseArrayLayer = 0;
		subRange.layerCount = 1;

	VkImageMemoryBarrier acquireBarrier = {};
		acquireBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		acquireBarrier.image = m_context.m_swapchainImages[image_index];
		acquireBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		acquireBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		acquireBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		acquireBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		acquireBarrier.srcAccessMask = 0; // quels acces memoires on attend avant la barrier
		acquireBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
		acquireBarrier.subresourceRange = subRange;

	//VkDependencyFlags dependencyFlags;
	vkCmdPipelineBarrier(m_rendercontext.m_currCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0/*dependencyFlags*/, 0, nullptr, 0, nullptr,1, &acquireBarrier);

	// on est bloque tant que on est pas en stage TRANSFER (car dstStage)
	// ici -> stage Transfer

	VkImageLayout swapLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	VkImage swapImage = m_context.m_swapchainImages[image_index]; 
	vkCmdClearColorImage(m_rendercontext.m_currCommandBuffer, swapImage, swapLayout, &clearColor, 1 , &subRange); // command de type TRANSFER

	// notre barrier bloque tout tant que le TRANFER ne s'est pas effectue
	VkImageMemoryBarrier presentBarrier = {};
		presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		presentBarrier.image = m_context.m_swapchainImages[image_index];
		presentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		presentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		presentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		presentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; // quels acces memoires on attend avant la barrier
		presentBarrier.dstAccessMask = 0;
		presentBarrier.subresourceRange = subRange;

	//VkDependencyFlags dependencyFlags;
	vkCmdPipelineBarrier(m_rendercontext.m_currCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0/*dependencyFlags*/, 0, nullptr, 0, nullptr, 1, &presentBarrier);
#else

	// ---------------------------------------------------------------------------------------------
	// Update MVP And Light
	static auto startTime = std::chrono::high_resolution_clock::now();
	auto currTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currTime - startTime).count();

	float nearClip = 0.1f;
	float farClip = 100.0f;
	float clipRange = farClip - nearClip;

	float minZ = nearClip;
	float maxZ = nearClip + clipRange;

	float range = maxZ - minZ;
	float ratio = maxZ / minZ;

	VP vp = {};
	//mvp.model = glm::rotate(glm::mat4(1.0f), (time * glm::radians(10.0f)), glm::vec3(0.0f,1.0f, 0.0f));
	//mvp.model += glm::translate(mvp.model, position);

	vp.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	vp.proj = glm::perspective(glm::radians(45.0f), m_context.m_swapchainExtent.width / (float)m_context.m_swapchainExtent.height, 0.1f, 100.0f);
	vp.proj[1][1] *= -1.f;

	vp.lightPos.x = 0.0f;
	vp.lightPos.y = 0.0f;
	vp.lightPos.z = 2.0f;

	// Cascade

	// Calculate split depths base on view camera frustum
	// https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
	// https://github.com/SaschaWillems/Vulkan/blob/master/examples/shadowmappingcascade/shadowmappingcascade.cpp

	float cascadeSplits[SHADOWMAP_CASCADE];
	float size = static_cast<float>(SHADOWMAP_CASCADE);
	float cascadeSplitLambda = 0.95f; // ?????

	for (uint32_t idxSplit = 0; idxSplit < SHADOWMAP_CASCADE; idxSplit++)
	{
		float p = (idxSplit + 1) / size;
		float log = minZ * std::pow(ratio,p);
		float uniform = minZ + range * p;
		float d = cascadeSplitLambda * (log - uniform) + uniform;
		cascadeSplits[idxSplit] = (d - nearClip) / clipRange;
	}

	// Calculate ortho 
	float lastSplitDist = 0.0f;
	for (uint32_t idxSplitDist = 0; idxSplitDist < SHADOWMAP_CASCADE; idxSplitDist++)
	{
		float splitDist = cascadeSplits[idxSplitDist];

		glm::vec3 frustumCorners[8] = {
				glm::vec3(-1.0f,  1.0f, -1.0f),
				glm::vec3(1.0f,  1.0f, -1.0f),
				glm::vec3(1.0f, -1.0f, -1.0f),
				glm::vec3(-1.0f, -1.0f, -1.0f),
				glm::vec3(-1.0f,  1.0f,  1.0f),
				glm::vec3(1.0f,  1.0f,  1.0f),
				glm::vec3(1.0f, -1.0f,  1.0f),
				glm::vec3(-1.0f, -1.0f,  1.0f),
		};

		// Project frustum corners into worldSpace
		glm::mat4 invCam = glm::inverse(vp.proj * vp.view);

		int frustumSize = 8;
		for (uint32_t idxFrustum = 0; idxFrustum < frustumSize; idxFrustum++)
		{
			glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[idxFrustum], 1.0f);
			frustumCorners[idxFrustum] = invCorner / invCorner.w;
		}

		int halfFrustumSize = frustumSize / 2;
		for (uint32_t idxFrustum = 0; idxFrustum < halfFrustumSize; idxFrustum++)
		{
			glm::vec3 dist = frustumCorners[halfFrustumSize + idxFrustum] - frustumCorners[idxFrustum];
			frustumCorners[halfFrustumSize + idxFrustum] = frustumCorners[idxFrustum] + (dist * splitDist);
			frustumCorners[idxFrustum] = frustumCorners[idxFrustum] + (dist * lastSplitDist);
		}

		// Get frustum center
		glm::vec3 frustumCenter = glm::vec3(0.0f);
		for (uint32_t idxCenter = 0; idxCenter < frustumSize; idxCenter++)
			frustumCenter += frustumCorners[idxCenter];

		frustumCenter /= frustumSize;

		float radius = 0.0f;
		for (uint32_t idxRadius = 0; idxRadius < frustumSize; idxRadius++)
		{
			float distance = glm::length(frustumCorners[idxRadius] - frustumCenter);
			radius = glm::max(radius, distance);
		}
		radius = std::ceil(radius * 16.0f) / 16.0f; // ??? 2x size frustum???

		glm::vec3 maxExtents = glm::vec3(radius);
		glm::vec3 minExtents = -maxExtents;

		// ONLY FOR DIRECTION LIGHT

		glm::vec3 light = glm::normalize(-vp.lightPos);

		//glm::mat4 lightProjectionMatrix = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
		//lightProjectionMatrix[1][1] *= -1.f;
		//vp.lightSpace = lightProjectionMatrix * lightViewMatrix;

		glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);
		lightOrthoMatrix[1][1] *= -1;
		glm::mat4 lightViewMatrix = glm::lookAt(
												frustumCenter - light * -minExtents.z, 
												frustumCenter, 
												glm::vec3(0.0f, 1.0f, 0.0f));
		
		vp.cascadeSplits[idxSplitDist] = (nearClip + splitDist * clipRange) * -1.0f;
		vp.lightSpace[idxSplitDist] = lightOrthoMatrix * lightViewMatrix;

		lastSplitDist = cascadeSplits[idxSplitDist];
	}


	/*vp.lightPos.x = cos(glm::radians(time * 360.0f)) * 40.0f;
	vp.lightPos.y = 50.0f + sin(glm::radians(time * 360.0f)) * 20.0f;
	vp.lightPos.z = 25.0f + sin(glm::radians(time * 360.0f)) * 5.0f;*/
	
	
	/*vp.lightPos.x = 5.0f;
	vp.lightPos.y = 5.0f;
	vp.lightPos.z = 0.0f;
	
	glm::mat4 depthProjectionMatrix = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
	depthProjectionMatrix[1][1] *= -1.f;
	glm::mat4 depthViewMatrix = glm::lookAt(vp.lightPos, glm::vec3(0.0f, 0.f, 0.f), glm::vec3(0.0f, 0.0f, -1.0f));

	vp.lightSpace = depthProjectionMatrix * depthViewMatrix;
	*/
	// ---------------------------------------------------------------------------------------------

	VkClearColorValue clearColor{ 0.55f, 0.55f, 0.55f, 1.0f };
	VkClearValue clearValues[2];

	////////////////// RENDER PASS -> SHADOW MAP (render light's pov) //////////////

	clearValues[0].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo shadowRenderPassBeginInfo = {};
	{
		shadowRenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		shadowRenderPassBeginInfo.renderPass = m_rendercontext.m_shadowRenderPass;
		//shadowRenderPassBeginInfo.framebuffer = m_rendercontext.m_shadowFrameBuffer;
		shadowRenderPassBeginInfo.renderArea.extent.width = m_context.m_shadowExtent.width;
		shadowRenderPassBeginInfo.renderArea.extent.height = m_context.m_shadowExtent.height;
		shadowRenderPassBeginInfo.clearValueCount = 1;
		shadowRenderPassBeginInfo.pClearValues = clearValues;
	}


	//vkCmdSetDepthBias(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,m_graphiquePipeline.m_pipelineShadow);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,m_graphiquePipeline.m_pipelineShadow);

	Buffer::UpdateUniformBuffer(m_context, image_index, sizeof(VP), 0, m_rendercontext.m_uniformBuffersMemory, &vp);
	
	for (size_t idxCascade = 0; idxCascade < SHADOWMAP_CASCADE; idxCascade++)
	{
		// Update framebuffer for cascade framebuffer
		shadowRenderPassBeginInfo.framebuffer = m_graphiquePipeline.m_cascades[idxCascade].m_frameBuffer;
		vkCmdBeginRenderPass(commandBuffer, &shadowRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		for (size_t idx = 0; idx < m_graphiquePipeline.m_scene.m_meshes.size(); idx++)
		{
			#pragma region MESH
			Mesh& mesh = m_graphiquePipeline.m_scene.m_meshes[idx];
			mesh.m_model.cascadeIdx = glm::vec4(idxCascade);
			#pragma endregion

			uint32_t dynamicOffsets[] = { (idxCascade * m_graphiquePipeline.m_scene.m_meshes.size() +idx) * 256 };
			Buffer::UpdateUniformBuffer(m_context, image_index, sizeof(Model), dynamicOffsets[0], m_rendercontext.m_uniformBuffersMemoryDynamic, &mesh.m_model);

			VkBuffer vertexBuffers[] = { mesh.m_buffer.m_vertexBuffer };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(commandBuffer, mesh.m_buffer.m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);

			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphiquePipeline.m_shadowPipelineLayout, 0, 1, &m_graphiquePipeline.m_descriptorSets[image_index], 1, dynamicOffsets);
			

			vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(mesh.m_indices.size()), 1, 0, 0, 0);
		}
		vkCmdEndRenderPass(commandBuffer);
	}
	////////////////////// SHADOW RENDER PASS END //////////////////////////////////////


	///////////////////// RENDER PASS -> SCENE RENDERING ///////////////////////////////
	///////////////////// Drawn uniform Buffer /////////////////////////////////////////

	clearValues[0].color = clearColor;
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo opaqueRenderPassBeginInfo = {};
	{
		opaqueRenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		opaqueRenderPassBeginInfo.renderPass = m_rendercontext.m_renderPass;
		opaqueRenderPassBeginInfo.framebuffer = m_rendercontext.m_frameBuffers[image_index];
		opaqueRenderPassBeginInfo.renderArea.extent = m_context.m_swapchainExtent;
		opaqueRenderPassBeginInfo.clearValueCount = 2;
		opaqueRenderPassBeginInfo.pClearValues = clearValues;
	}
	vkCmdBeginRenderPass(commandBuffer,&opaqueRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	
	// [...]

	//vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,m_graphiquePipeline.m_pipelineLayout, 0, 1, &m_graphiquePipeline.m_descriptorSet[image_index], 0, nullptr);
	
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphiquePipeline.m_pipelineOpaque);
	
	Buffer::UpdateUniformBuffer(m_context, image_index, sizeof(VP),0,m_rendercontext.m_uniformBuffersMemory, &vp);
	
	for (size_t idx = 0; idx < m_graphiquePipeline.m_scene.m_meshes.size(); idx++)
	{
		Mesh& mesh = m_graphiquePipeline.m_scene.m_meshes[idx];

		/*if (idx == 2)
			mesh.m_model.model = glm::rotate(mesh.m_model.model, glm::radians(40.0f) * time, glm::vec3(1.0f, 0.0f, 0.0f));*/
	
		VkBuffer vertexBuffers[] = { mesh.m_buffer.m_vertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		uint32_t dynamicOffsets[] = { (20 + idx) * 256 };
		
		////// OPAQUE /////////
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphiquePipeline.m_pipelineLayout, 0, 1, &m_graphiquePipeline.m_descriptorSets[image_index], 1, dynamicOffsets);
		
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(commandBuffer, mesh.m_buffer.m_indexBuffer ,0 ,VK_INDEX_TYPE_UINT32);
	
		Buffer::UpdateUniformBuffer(m_context, image_index, sizeof(Model), dynamicOffsets[0] ,m_rendercontext.m_uniformBuffersMemoryDynamic, &mesh.m_model);

		vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(mesh.m_indices.size()), 1, 0, 0,0);
		//vkCmdDraw(m_rendercontext.m_currCommandBuffer, static_cast<uint32_t>(vertices.size()) 1, 0, 0);
		/////////////////////
	}

	m_imgui.DrawFrame(m_rendercontext, m_context, commandBuffer);
	vkCmdEndRenderPass(commandBuffer);
		
	////////////////////// SCENE RENDER PASS END //////////////////////////////////////


#endif

	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
	VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		submitInfo.pWaitDstStageMask = &stageMask;
		submitInfo.pWaitSemaphores = aquireSem; // on attend acquire
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = presentSem; // on signal presentSem des qu'on est pret
		submitInfo.signalSemaphoreCount = 1;
	DEBUG_CHECK_VK(vkQueueSubmit(m_rendercontext.m_graphicsQueue, 1, &submitInfo, m_rendercontext.m_mainFences[m_currentFrame]));
	// la fence precedente se dignale des que le command buffer est totalement traite par la queue

	VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;			// OFF, pas forcement necessaire ici
		presentInfo.pWaitSemaphores = presentSem;	// presentation semaphore ici synchro avec le dernier vkQueueSubmit;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &m_context.m_swapchain;
		presentInfo.pImageIndices = &image_index;
	DEBUG_CHECK_VK(vkQueuePresentKHR(m_rendercontext.m_presentQueue, &presentInfo));

	m_frame++;
	m_currentFrame = m_frame % m_rendercontext.PENDING_FRAMES;

	return true;
}

int main(void)
{
	/* Initialize the library */
	if (!glfwInit())
		return -1;
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	VulkanGraphicsApplication app;

	/* Create a windowed mode window and its OpenGL context */
	app.m_window = glfwCreateWindow(800, 600, "00_minimal", NULL, NULL);
	if (!app.m_window)
	{
		glfwTerminate();
		return -1;
	}

	app.Initialize();
	


	/* Loop until the user closes the window */
	while (!glfwWindowShouldClose(app.m_window))
	{
		/* Render here */
		app.Display();

		/* Poll for and process events */
		glfwPollEvents();
	}

	app.m_imgui.Clean(app.m_context);
	app.Shutdown();

	glfwTerminate();
	return 0;
}