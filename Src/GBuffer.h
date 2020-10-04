#pragma once
#include <vector>

#include <vulkan/vulkan.h>

#include "Camera.h"
#include "Renderable.h"

class GBuffer {
	int buffer_width, buffer_height;

	std::vector<VkFramebuffer> frame_buffers;
	
	VkRenderPass render_pass;
	
	VkPipeline       pipeline;
	VkPipelineLayout pipeline_layout;
	
	VkDescriptorPool             descriptor_pool;
	VkDescriptorSetLayout        descriptor_set_layout;
	std::vector<VkDescriptorSet> descriptor_sets;

public:
	struct FrameBuffer {
		struct Attachment {
			VkImage        image;
			VkImageView    image_view;
			VkDeviceMemory memory;
		};

		std::vector<Attachment> attachments;
		VkFormat                format;

		void init(int swapchain_image_count, int width, int height, VkFormat format, VkImageUsageFlagBits usage);
		void free();
	};
	
	FrameBuffer frame_buffer_albedo;
	FrameBuffer frame_buffer_position;
	FrameBuffer frame_buffer_normal;
	FrameBuffer frame_buffer_depth;
	
	std::vector<VkCommandBuffer> command_buffers;
	
	void init(int swapchain_image_count, int width, int height, std::vector<Renderable> const & renderables, std::vector<Texture *> const & textures);
	void free();

	void record_command_buffer(int image_index, int width, int height, Camera const & camera, std::vector<Renderable> const & renderables);
};
