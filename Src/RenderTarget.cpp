#include "RenderTarget.h"

#include <cassert>

#include "VulkanCheck.h"
#include "VulkanMemory.h"
#include "VulkanContext.h"

void RenderTarget::init(int width, int height, VkFormat format, VkImageUsageFlagBits usage) {
	VkImageAspectFlags image_aspect_mask = 0;
	VkImageLayout      image_layout;

	this->format = format;

	if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
		image_aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_layout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}

	if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
		image_aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		image_layout      = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}

	assert(image_aspect_mask != 0);
	
	auto device = VulkanContext::get_device();

	VkImageCreateInfo image_create_info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	image_create_info.imageType = VK_IMAGE_TYPE_2D;
	image_create_info.format = format;
	image_create_info.extent = { u32(width), u32(height), 1 };
	image_create_info.mipLevels = 1;
	image_create_info.arrayLayers = 1;
	image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_create_info.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT;

	VK_CHECK(vkCreateImage(device, &image_create_info, nullptr, &image));

	VkMemoryRequirements memory_requirements; vkGetImageMemoryRequirements(device, image, &memory_requirements);

	VkMemoryAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = VulkanMemory::find_memory_type(memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	
	VK_CHECK(vkAllocateMemory(device, &alloc_info, nullptr, &memory));

	VK_CHECK(vkBindImageMemory(device, image, memory, 0));

	VkImageViewCreateInfo image_view_create_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	image_view_create_info.format = format;
	image_view_create_info.subresourceRange = { };
	image_view_create_info.subresourceRange.aspectMask = image_aspect_mask;
	image_view_create_info.subresourceRange.baseMipLevel = 0;
	image_view_create_info.subresourceRange.levelCount   = 1;
	image_view_create_info.subresourceRange.baseArrayLayer = 0;
	image_view_create_info.subresourceRange.layerCount     = 1;
	image_view_create_info.image = image;

	VK_CHECK(vkCreateImageView(device, &image_view_create_info, nullptr, &image_view));
}

void RenderTarget::free() {
	auto device = VulkanContext::get_device();

	vkDestroyImage    (device, image,      nullptr);
	vkDestroyImageView(device, image_view, nullptr);
	vkFreeMemory      (device, memory,     nullptr);
}
