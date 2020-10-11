#pragma once
#include <vector>

#include <vulkan/vulkan.h>

#include "Scene.h"
#include "RenderTarget.h"

class GBuffer {
	int width, height;

	VkFramebuffer frame_buffer;
	
	VkRenderPass render_pass;
	
	struct {
		VkPipeline geometry;
		VkPipeline sky;
	} pipelines;

	struct {
		VkPipelineLayout geometry;
		VkPipelineLayout sky;
	} pipeline_layouts;

	VkDescriptorPool descriptor_pool;

	struct {
		VkDescriptorSetLayout geometry;
		VkDescriptorSetLayout sky;
	} descriptor_set_layouts;
	
	std::vector<VulkanMemory::Buffer> uniform_buffers;
	std::vector<VkDescriptorSet>      descriptor_sets;

public:
	RenderTarget render_target_albedo;
	RenderTarget render_target_position;
	RenderTarget render_target_normal;
	RenderTarget render_target_depth;
	
	std::vector<VkCommandBuffer> command_buffers;
	
	void init(int swapchain_image_count, int width, int height);
	void free();

	void record_command_buffer(int image_index, Scene const & scene);
};
