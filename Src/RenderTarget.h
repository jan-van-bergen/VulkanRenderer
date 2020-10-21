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
	VkSampler     sampler;

	std::vector<VkClearValue> clear_values;

	void add_attachment(int width, int height, VkFormat format, unsigned usage, VkImageLayout image_layout, VkClearValue clear_value);

	void init(int width, int height, VkRenderPass render_pass, VkFilter filter = VK_FILTER_NEAREST);
	void free();

	std::vector<VkAttachmentDescription> get_attachment_descriptions();
};
