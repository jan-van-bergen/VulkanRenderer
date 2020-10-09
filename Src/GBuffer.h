#pragma once
#include <vector>

#include <vulkan/vulkan.h>

#include "Camera.h"
#include "Mesh.h"
#include "Texture.h"
#include "RenderTarget.h"

class GBuffer {
	int width, height;

	VkFramebuffer frame_buffer;
	
	VkRenderPass render_pass;
	
	VkPipeline       pipeline;
	VkPipelineLayout pipeline_layout;
	
	VkDescriptorPool      descriptor_pool;
	VkDescriptorSetLayout descriptor_set_layout;
	
public:
	RenderTarget render_target_albedo;
	RenderTarget render_target_position;
	RenderTarget render_target_normal;
	RenderTarget render_target_depth;
	
	std::vector<VkCommandBuffer> command_buffers;
	
	void init(int swapchain_image_count, int width, int height);
	void free();

	void record_command_buffer(int image_index, Camera const & camera, std::vector<MeshInstance> const & meshes);
};
