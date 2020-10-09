#pragma once
#include <vulkan/vulkan.h>

struct RenderTarget {
	VkImage        image;
	VkImageView    image_view;
	VkDeviceMemory memory;
	VkFormat       format;

	void init(int width, int height, VkFormat format, VkImageUsageFlagBits usage);
	void free();
};
