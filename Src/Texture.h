#pragma once
#include <string>

#include <vulkan/vulkan.h>

struct Texture {
	VkImage        image;
	VkDeviceMemory image_memory;
	VkImageView    image_view;
	VkSampler      sampler;

	VkDescriptorSet descriptor_set;

	static Texture * load(std::string const & filename);

	static void free();
};
