#include "RenderTarget.h"

#include <cassert>

#include "VulkanCheck.h"
#include "VulkanMemory.h"
#include "VulkanContext.h"

#include "Util.h"

void RenderTarget::add_attachment(int width, int height, VkFormat format, VkImageUsageFlagBits usage, VkImageLayout image_layout) {
	auto device = VulkanContext::get_device();
	
	VkImageAspectFlags image_aspect_mask = 0;

	if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)         image_aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
	if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) image_aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;

	assert(image_aspect_mask != 0);

	auto & attachment = attachments.emplace_back();
	attachment.format = format;

	VkImageCreateInfo image_create_info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	image_create_info.imageType = VK_IMAGE_TYPE_2D;
	image_create_info.format = format;
	image_create_info.extent = { u32(width), u32(height), 1 };
	image_create_info.mipLevels = 1;
	image_create_info.arrayLayers = 1;
	image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_create_info.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT;

	VK_CHECK(vkCreateImage(device, &image_create_info, nullptr, &attachment.image));

	VkMemoryRequirements memory_requirements; vkGetImageMemoryRequirements(device, attachment.image, &memory_requirements);

	VkMemoryAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = VulkanMemory::find_memory_type(memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VK_CHECK(vkAllocateMemory(device, &alloc_info, nullptr, &attachment.memory));

	VK_CHECK(vkBindImageMemory(device, attachment.image, attachment.memory, 0));

	VkImageViewCreateInfo image_view_create_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	image_view_create_info.format = format;
	image_view_create_info.subresourceRange = { };
	image_view_create_info.subresourceRange.aspectMask = image_aspect_mask;
	image_view_create_info.subresourceRange.baseMipLevel = 0;
	image_view_create_info.subresourceRange.levelCount   = 1;
	image_view_create_info.subresourceRange.baseArrayLayer = 0;
	image_view_create_info.subresourceRange.layerCount     = 1;
	image_view_create_info.image = attachment.image;

	VK_CHECK(vkCreateImageView(device, &image_view_create_info, nullptr, &attachment.image_view));

	attachment.description.format = format;
	attachment.description.samples = VK_SAMPLE_COUNT_1_BIT;
	attachment.description.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment.description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment.description.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment.description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment.description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachment.description.finalLayout = image_layout;
}

void RenderTarget::init(int width, int height) {
	auto device = VulkanContext::get_device();

	std::vector<VkAttachmentDescription> attachment_descs(attachments.size());
	std::vector<VkImageView>             attachment_views(attachments.size());
	std::vector<VkAttachmentReference> refs_colour;
	std::vector<VkAttachmentReference> refs_depth;

	for (int i = 0; i < attachments.size(); i++) {
		attachment_descs[i] = attachments[i].description;
		attachment_views[i] = attachments[i].image_view;

		auto depth_formats = {
			VK_FORMAT_D16_UNORM,
			VK_FORMAT_X8_D24_UNORM_PACK32,
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D16_UNORM_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D32_SFLOAT_S8_UINT
		};
		bool is_depth_format = std::find(depth_formats.begin(), depth_formats.end(), attachments[i].format) != depth_formats.end();

		if (is_depth_format) {
			refs_depth.push_back({ u32(i), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL });
		} else {
			refs_colour.push_back({ u32(i), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
		}
	}
	
	// Create Renderpass
	VkSubpassDescription subpass = { };
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = refs_colour.size();
	subpass.pColorAttachments    = refs_colour.data();
	subpass.pDepthStencilAttachment = refs_depth.size() > 0 ? refs_depth.data() : nullptr;

	VkSubpassDependency dependencies[2] = { };
	
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo render_pass_create_info = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	render_pass_create_info.attachmentCount = attachment_descs.size();
	render_pass_create_info.pAttachments    = attachment_descs.data();
	render_pass_create_info.subpassCount = 1;
	render_pass_create_info.pSubpasses   = &subpass;
	render_pass_create_info.dependencyCount = Util::array_element_count(dependencies);
	render_pass_create_info.pDependencies   = dependencies;

	VK_CHECK(vkCreateRenderPass(device, &render_pass_create_info, nullptr, &render_pass));

	// Create Frame Buffer
	frame_buffer = VulkanContext::create_frame_buffer(width, height, render_pass, attachment_views);

	// Create Sampler to sample from the GBuffer's color attachments
	VkSamplerCreateInfo sampler_create_info = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	sampler_create_info.magFilter = VK_FILTER_NEAREST;
	sampler_create_info.minFilter = VK_FILTER_NEAREST;
	sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_create_info.mipLodBias = 0.0f;
	sampler_create_info.maxAnisotropy = 1.0f;
	sampler_create_info.minLod = 0.0f;
	sampler_create_info.maxLod = 1.0f;
	sampler_create_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

	VK_CHECK(vkCreateSampler(device, &sampler_create_info, nullptr, &sampler));
}

void RenderTarget::free() {
	auto device = VulkanContext::get_device();

	for (auto & attachment : attachments) {
		vkDestroyImage    (device, attachment.image,      nullptr);
		vkDestroyImageView(device, attachment.image_view, nullptr);
		vkFreeMemory      (device, attachment.memory,     nullptr);
	}

	vkDestroyFramebuffer(device, frame_buffer, nullptr);
	vkDestroyRenderPass (device, render_pass,  nullptr);
	vkDestroySampler    (device, sampler,      nullptr);
}
