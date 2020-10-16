#pragma once
#include "VulkanMemory.h"

#include "Scene.h"
#include "RenderTarget.h"

struct RenderTaskGBuffer {
private:
	int width;
	int height;

	Scene const & scene;
	
	struct {
		VkDescriptorSetLayout geometry;
		VkDescriptorSetLayout sky;
	} descriptor_set_layouts;
	
	struct {
		VkPipelineLayout geometry;
		VkPipelineLayout sky;
	} pipeline_layouts;

	struct {
		VkPipeline geometry;
		VkPipeline sky;
	} pipelines;

	std::vector<VulkanMemory::Buffer> uniform_buffers;
	std::vector<VkDescriptorSet>      descriptor_sets_sky;

	RenderTarget render_target;
	VkRenderPass render_pass;

public:
	RenderTaskGBuffer(Scene const & scene) : scene(scene) { }

	void init(VkDescriptorPool descriptor_pool, int width, int height, int swapchain_image_count);
	void free();

	void render(int image_index, VkCommandBuffer command_buffer);

	RenderTarget const & get_render_target() { return render_target; }
};
