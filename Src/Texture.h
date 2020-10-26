#pragma once
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

typedef int TextureHandle;

struct Texture {
	VkImage        image;
	VkDeviceMemory image_memory;
	VkImageView    image_view;
	VkSampler      sampler;

	VkDescriptorSet descriptor_set;

	void generate_mipmaps(int width, int height, int mip_levels);
};
