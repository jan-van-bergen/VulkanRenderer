#pragma once
#include <vulkan/vulkan.h>

#include "Types.h"

namespace VulkanMemory {
	struct Buffer {
		VkBuffer       buffer;
		VkDeviceMemory memory;
	};

	VkCommandBuffer command_buffer_single_use_begin();
	void            command_buffer_single_use_end(VkCommandBuffer command_buffer);

	u32 find_memory_type(u32 type_filter, VkMemoryPropertyFlags properties);

	Buffer buffer_create(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
	void   buffer_free(Buffer & buffer);

	void buffer_copy_staged(Buffer const & buffer_dst, void const * data_src, size_t size);
	void buffer_copy_direct(Buffer const & buffer_dst, void const * data_src, size_t size);

	void * buffer_map  (Buffer const & buffer_dst, size_t size);
	void   buffer_unmap(Buffer const & buffer_dst);

	void        create_image(u32 width, u32 height, u32 mip_levels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage & image, VkDeviceMemory & image_memory);
	VkImageView create_image_view(VkImage image, u32 mip_levels, VkFormat format, VkImageAspectFlags aspect_mask);

	void transition_image_layout(VkImage image, u32 mip_levels, VkFormat format, VkImageLayout layout_old, VkImageLayout layout_new);

	void buffer_copy_to_image(VkBuffer buffer, VkImage image, u32 width, u32 height);
}
