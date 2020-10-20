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
		VkDescriptorSetLayout material;
		VkDescriptorSetLayout bones;
		VkDescriptorSetLayout sky;
	} descriptor_set_layouts;
	
	struct {
		VkPipelineLayout geometry_static;
		VkPipelineLayout geometry_animated;
		VkPipelineLayout sky;
	} pipeline_layouts;

	struct {
		VkPipeline geometry_static;
		VkPipeline geometry_animated;
		VkPipeline sky;
	} pipelines;

	struct {
		std::vector<VulkanMemory::Buffer> material;
		std::vector<VulkanMemory::Buffer> bones;
		std::vector<VulkanMemory::Buffer> sky;
	} uniform_buffers;

	struct {
		std::vector<VkDescriptorSet> material;
		std::vector<VkDescriptorSet> bones;
		std::vector<VkDescriptorSet> sky;
	} descriptor_sets;

	RenderTarget render_target;
	VkRenderPass render_pass;

public:
	RenderTaskGBuffer(Scene const & scene) : scene(scene) { }

	void init(VkDescriptorPool descriptor_pool, int width, int height, int swapchain_image_count);
	void free();

	void render(int image_index, VkCommandBuffer command_buffer);

	RenderTarget const & get_render_target() { return render_target; }
};
