#pragma once
#include <vulkan/vulkan.h>

#include "Types.h"

namespace VulkanMemory {
	VkCommandBuffer command_buffer_single_use_begin();
	void            command_buffer_single_use_end(VkCommandBuffer command_buffer);

	u32 find_memory_type(u32 type_filter, VkMemoryPropertyFlags properties);

	void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer & buffer, VkDeviceMemory & buffer_memory);

	void buffer_copy(VkBuffer buffer_dst, VkBuffer buffer_src, VkDeviceSize size);
	void buffer_memory_copy(VkDeviceMemory device_memory, void const * data, u64 size);

	void create_image(u32 width, u32 height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage & image, VkDeviceMemory & image_memory);
	VkImageView create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect_mask);

	void transition_image_layout(VkImage image, VkFormat format, VkImageLayout layout_old, VkImageLayout layout_new);

	void buffer_copy_to_image(VkBuffer buffer, VkImage image, u32 width, u32 height);
}
