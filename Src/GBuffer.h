#pragma once
#include <vector>

#include <vulkan/vulkan.h>

#include "Camera.h"
#include "Mesh.h"
#include "Texture.h"

class GBuffer {
	int width, height;

	VkFramebuffer frame_buffer;
	
	VkRenderPass render_pass;
	
	VkPipeline       pipeline;
	VkPipelineLayout pipeline_layout;
	
	VkDescriptorPool      descriptor_pool;
	VkDescriptorSetLayout descriptor_set_layout;
	
public:
	struct FrameBuffer {
		VkImage        image;
		VkImageView    image_view;
		VkDeviceMemory memory;
		VkFormat       format;

		void init(int swapchain_image_count, int width, int height, VkFormat format, VkImageUsageFlagBits usage);
		void free();
	};
	
	FrameBuffer frame_buffer_albedo;
	FrameBuffer frame_buffer_position;
	FrameBuffer frame_buffer_normal;
	FrameBuffer frame_buffer_depth;
	
	std::vector<VkCommandBuffer> command_buffers;
	
	void init(int swapchain_image_count, int width, int height);
	void free();

	void record_command_buffer(int image_index, Camera const & camera, std::vector<MeshInstance> const & meshes);
};
