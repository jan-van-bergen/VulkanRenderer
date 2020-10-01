#include "Texture.h"

#include <memory>
#include <unordered_map>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#include "VulkanCheck.h"
#include "VulkanContext.h"
#include "VulkanMemory.h"

static std::unordered_map<std::string, std::unique_ptr<Texture>> cache;

Texture * Texture::load(std::string const & filename) {
	auto & texture = cache[filename];

	if (texture != nullptr) return texture.get();

	int texture_width;
	int texture_height;
	int texture_channels;

	auto pixels = stbi_load(filename.c_str(), &texture_width, &texture_height, &texture_channels, STBI_rgb_alpha);
	if (!pixels) {
		printf("ERROR: Unable to load Texture!\n");
		abort();
	}

	VkDeviceSize texture_size = texture_width * texture_height * 4;

	VulkanMemory::Buffer staging_buffer = VulkanMemory::buffer_create(texture_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	VulkanMemory::buffer_copy_direct(staging_buffer, pixels, texture_size);

	stbi_image_free(pixels);

	texture = std::make_unique<Texture>();

	VulkanMemory::create_image(
		texture_width,
		texture_height,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		texture->image,
		texture->image_memory
	);

	VulkanMemory::transition_image_layout(texture->image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	VulkanMemory::buffer_copy_to_image(staging_buffer.buffer, texture->image, texture_width, texture_height);

	VulkanMemory::transition_image_layout(texture->image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	auto device = VulkanContext::get_device();

	VulkanMemory::buffer_free(staging_buffer);

	texture->image_view = VulkanMemory::create_image_view(texture->image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
	
	VkSamplerCreateInfo sampler_create_info = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	sampler_create_info.magFilter = VK_FILTER_LINEAR;
	sampler_create_info.minFilter = VK_FILTER_LINEAR;
	sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_create_info.anisotropyEnable = VK_TRUE;
	sampler_create_info.maxAnisotropy    = 16.0f;
	sampler_create_info.unnormalizedCoordinates = VK_FALSE;
	sampler_create_info.compareEnable = VK_FALSE;
	sampler_create_info.compareOp     = VK_COMPARE_OP_ALWAYS;
	sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_create_info.mipLodBias = 0.0f;
	sampler_create_info.minLod     = 0.0f;
	sampler_create_info.maxLod     = 0.0f;

	VK_CHECK(vkCreateSampler(device, &sampler_create_info, nullptr, &texture->sampler));

	return texture.get();
}

void Texture::free() {
	auto device = VulkanContext::get_device();

	for (auto & kvp : cache) {
		auto & texture = kvp.second;

		vkDestroySampler  (device, texture->sampler,      nullptr);
		vkDestroyImageView(device, texture->image_view,   nullptr);
		vkDestroyImage    (device, texture->image,        nullptr);
		vkFreeMemory      (device, texture->image_memory, nullptr);
	}

	cache.clear();
}
