#include "VulkanMemory.h"

#include "VulkanCheck.h"
#include "VulkanContext.h"

u32 VulkanMemory::find_memory_type(u32 type_filter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memory_properties;
	vkGetPhysicalDeviceMemoryProperties(VulkanContext::get_physical_device(), &memory_properties);

	for (u32 i = 0; i < memory_properties.memoryTypeCount; i++) {
		if ((type_filter & (1 << i)) && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	printf("Unable to find memory type %i!\n", type_filter);
	abort();
}

VulkanMemory::Buffer VulkanMemory::buffer_create(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
	Buffer buffer;

	VkBufferCreateInfo buffer_create_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	buffer_create_info.size = size;
	buffer_create_info.usage = usage;
	buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkDevice device = VulkanContext::get_device();

	VK_CHECK(vkCreateBuffer(device, &buffer_create_info, nullptr, &buffer.buffer));

	VkMemoryRequirements requirements;
	vkGetBufferMemoryRequirements(device, buffer.buffer, &requirements);

	VkMemoryAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	alloc_info.allocationSize = requirements.size;
	alloc_info.memoryTypeIndex = find_memory_type(requirements.memoryTypeBits, properties);

	VK_CHECK(vkAllocateMemory(device, &alloc_info, nullptr, &buffer.memory));

	VK_CHECK(vkBindBufferMemory(device, buffer.buffer, buffer.memory, 0));

	return buffer;
}

void VulkanMemory::buffer_free(Buffer & buffer) {
	VkDevice device = VulkanContext::get_device();

	vkDestroyBuffer(device, buffer.buffer, nullptr);
	vkFreeMemory   (device, buffer.memory, nullptr);
}

void VulkanMemory::buffer_copy_staged(Buffer const & buffer_dst, void const * data_src, size_t size) {	
	// Create temporary staging buffer
	VulkanMemory::Buffer staging_buffer = VulkanMemory::buffer_create(
		size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	buffer_copy_direct(staging_buffer, data_src, size);

	// Copy Staging Buffer over to desired destination Buffer
	VkCommandBuffer copy_command_buffer = command_buffer_single_use_begin();

	VkBufferCopy buffer_copy = { };
	buffer_copy.srcOffset = 0;
	buffer_copy.dstOffset = 0;
	buffer_copy.size = size;
	vkCmdCopyBuffer(copy_command_buffer, staging_buffer.buffer, buffer_dst.buffer, 1, &buffer_copy);

	command_buffer_single_use_end(copy_command_buffer);

	buffer_free(staging_buffer);
}

void VulkanMemory::buffer_copy_direct(Buffer const & buffer_dst, void const * data_src, size_t size) {
	VkDevice device = VulkanContext::get_device();

	void * dst = buffer_map(buffer_dst, size);
	memcpy(dst, data_src, size);
	buffer_unmap(buffer_dst);
}

void * VulkanMemory::buffer_map(Buffer const & buffer, size_t size) {
	void * mapped; vkMapMemory(VulkanContext::get_device(), buffer.memory, 0, size, 0, &mapped);
	return mapped;
}

void VulkanMemory::buffer_unmap(Buffer const & buffer) {
	vkUnmapMemory(VulkanContext::get_device(), buffer.memory);
}

VkCommandBuffer VulkanMemory::command_buffer_single_use_begin() {
	VkCommandBufferAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandPool = VulkanContext::get_command_pool();
	alloc_info.commandBufferCount = 1;

	VkCommandBuffer command_buffer;
	VK_CHECK(vkAllocateCommandBuffers(VulkanContext::get_device(), &alloc_info, &command_buffer));

	VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK(vkBeginCommandBuffer(command_buffer, &begin_info));

	return command_buffer;
}

void VulkanMemory::command_buffer_single_use_end(VkCommandBuffer command_buffer) {
	vkEndCommandBuffer(command_buffer);

	VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;

	VkQueue queue_graphics = VulkanContext::get_queue_graphics();

	VK_CHECK(vkQueueSubmit(queue_graphics, 1, &submit_info, VK_NULL_HANDLE));
	VK_CHECK(vkQueueWaitIdle(queue_graphics));

	vkFreeCommandBuffers(VulkanContext::get_device(), VulkanContext::get_command_pool(), 1, &command_buffer);
}

void VulkanMemory::create_image(u32 width, u32 height, u32 mip_levels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage & image, VkDeviceMemory & image_memory) {
	VkImageCreateInfo image_create_info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	image_create_info.imageType = VK_IMAGE_TYPE_2D;
	image_create_info.extent.width  = width;
	image_create_info.extent.height = height;
	image_create_info.extent.depth  = 1;
	image_create_info.mipLevels   = mip_levels;
	image_create_info.arrayLayers = 1;
	image_create_info.format = format;
	image_create_info.tiling = tiling;
	image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_create_info.usage         = usage;
	image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_create_info.flags = 0;
	
	VkDevice device = VulkanContext::get_device();

	VK_CHECK(vkCreateImage(device, &image_create_info, nullptr, &image));

	VkMemoryRequirements requirements;
	vkGetImageMemoryRequirements(device, image, &requirements);

	VkMemoryAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	alloc_info.allocationSize = requirements.size;
	alloc_info.memoryTypeIndex = find_memory_type(requirements.memoryTypeBits, properties);

	VK_CHECK(vkAllocateMemory(device, &alloc_info, nullptr, &image_memory));

	VK_CHECK(vkBindImageMemory(device, image, image_memory, 0));
}

VkImageView VulkanMemory::create_image_view(VkImage image, u32 mip_levels, VkFormat format, VkImageAspectFlags aspect_mask) {
	VkImageViewCreateInfo image_view_create_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	image_view_create_info.image = image;
	image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	image_view_create_info.format = format;
	
	image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	image_view_create_info.subresourceRange.aspectMask = aspect_mask;
	image_view_create_info.subresourceRange.baseMipLevel = 0;
	image_view_create_info.subresourceRange.levelCount   = mip_levels;
	image_view_create_info.subresourceRange.baseArrayLayer = 0;
	image_view_create_info.subresourceRange.layerCount     = 1;

	VkImageView image_view;
	VK_CHECK(vkCreateImageView(VulkanContext::get_device(), &image_view_create_info, nullptr, &image_view));

	return image_view;
}

void VulkanMemory::transition_image_layout(VkImage image, u32 mip_levels, VkFormat format, VkImageLayout layout_old, VkImageLayout layout_new) {
	VkCommandBuffer command_buffer = command_buffer_single_use_begin();

	VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.oldLayout = layout_old;
	barrier.newLayout = layout_new;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount   = mip_levels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount     = 1;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = 0;

	VkPipelineStageFlags stage_src;
	VkPipelineStageFlags stage_dst;

	if (layout_old == VK_IMAGE_LAYOUT_UNDEFINED && layout_new == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		stage_src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		stage_dst = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if (layout_old == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && layout_new == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		stage_src = VK_PIPELINE_STAGE_TRANSFER_BIT;
		stage_dst = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else if (layout_old == VK_IMAGE_LAYOUT_UNDEFINED && layout_new == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		stage_src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		stage_dst = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	} else {
		printf("ERROR: unsupported layout transition!");
		abort();
	}

	vkCmdPipelineBarrier(command_buffer, stage_src, stage_dst, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	command_buffer_single_use_end(command_buffer);
}

void VulkanMemory::buffer_copy_to_image(VkBuffer buffer, VkImage image, u32 width, u32 height) {
	VkCommandBuffer command_buffer = command_buffer_single_use_begin();

	VkBufferImageCopy region = { };
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount     = 1;

	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = { width, height, 1 };

	vkCmdCopyBufferToImage(
		command_buffer,
		buffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region
	);

	command_buffer_single_use_end(command_buffer);
}
