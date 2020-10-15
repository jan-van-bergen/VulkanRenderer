#pragma once
#include <vector>

#include <vulkan/vulkan.h>

struct RenderTarget {
	struct Attachment {
		friend RenderTarget;

		VkImage        image;
		VkImageView    image_view;
		VkDeviceMemory memory;
		VkFormat       format;

		private: VkAttachmentDescription description;
	};
	std::vector<Attachment> attachments;

	VkFramebuffer frame_buffer;
	VkRenderPass  render_pass;
	VkSampler     sampler;

	void add_attachment(int width, int height, VkFormat format, VkImageUsageFlagBits usage, VkImageLayout image_layout);

	void init(int width, int height);
	void free();
};
