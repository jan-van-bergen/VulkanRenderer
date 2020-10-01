#include "Texture.h"

#include <memory>
#include <unordered_map>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#include "VulkanCheck.h"
#include "VulkanContext.h"
#include "VulkanMemory.h"

static std::unordered_map<std::string, std::unique_ptr<Texture>> cache;

static void generate_mipmaps(Texture * texture, u32 width, u32 height, u32 mip_levels) {
	VkCommandBuffer command_buffer = VulkanMemory::command_buffer_single_use_begin();

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = texture->image;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	int level_width  = width  / 2;
	int level_height = height / 2;

	// Downsample iteratively
	for (int i = 1; i < mip_levels; i++) {
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(command_buffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		// Blit using linear filtering to downsample previous level
		VkImageBlit blit = { };
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { level_width * 2, level_height * 2, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { level_width, level_height, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;

		vkCmdBlitImage(command_buffer,
			texture->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			VK_FILTER_LINEAR
		);

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(command_buffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		if (level_width  > 1) level_width  /= 2;
		if (level_height > 1) level_height /= 2;
	}

	// Transition Texture into Shader optimal layout
	barrier.subresourceRange.baseMipLevel = mip_levels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(command_buffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	VulkanMemory::command_buffer_single_use_end(command_buffer);
}

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

	auto texture_size = texture_width * texture_height * 4;
	
	auto staging_buffer = VulkanMemory::buffer_create(texture_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	VulkanMemory::buffer_copy_direct(staging_buffer, pixels, texture_size);

	stbi_image_free(pixels);

	texture = std::make_unique<Texture>();

	auto mip_levels = 1 + u32(std::log2(std::max(texture_width, texture_height)));

	VulkanMemory::create_image(
		texture_width,
		texture_height,
		mip_levels,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		texture->image,
		texture->image_memory
	);

	VulkanMemory::transition_image_layout(texture->image, mip_levels, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	VulkanMemory::buffer_copy_to_image(staging_buffer.buffer, texture->image, texture_width, texture_height);

	generate_mipmaps(texture.get(), texture_width, texture_height, mip_levels);

	auto device = VulkanContext::get_device();

	VulkanMemory::buffer_free(staging_buffer);

	texture->image_view = VulkanMemory::create_image_view(texture->image, mip_levels, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
	
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
	sampler_create_info.maxLod     = float(mip_levels);

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
