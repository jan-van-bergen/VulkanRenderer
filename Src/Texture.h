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

	static inline std::vector<Texture> textures;

	static TextureHandle load(std::string const & filename);

	static void free();
};
