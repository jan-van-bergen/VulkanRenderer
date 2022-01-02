#pragma once
#include "VulkanMemory.h"

#include "Scene.h"
#include "RenderTarget.h"

struct RenderTaskGBuffer {
private:
	int width;
	int height;

	Scene & scene;

	struct {
		VkDescriptorSetLayout cull;
		VkDescriptorSetLayout geometry;
		VkDescriptorSetLayout material;
		VkDescriptorSetLayout bones;
		VkDescriptorSetLayout sky;
	} descriptor_set_layouts;

	struct {
		VkPipelineLayout cull;
		VkPipelineLayout geometry_static;
		VkPipelineLayout geometry_animated;
		VkPipelineLayout sky;
	} pipeline_layouts;

	struct {
		VkPipeline cull;
		VkPipeline geometry_static;
		VkPipeline geometry_animated;
		VkPipeline sky;
	} pipelines;

	struct {
		std::vector<VulkanMemory::Buffer> camera;
		std::vector<VulkanMemory::Buffer> material;
		std::vector<VulkanMemory::Buffer> sky;
	} uniform_buffers;

	struct {
		std::vector<VulkanMemory::Buffer> cull_commands;
		std::vector<VulkanMemory::Buffer> cull_stats;
		std::vector<VulkanMemory::Buffer> cull_model;
	} storage_buffers;

	struct {
		std::vector<VkDescriptorSet> cull;
		std::vector<VkDescriptorSet> material;
		std::vector<VkDescriptorSet> bones;
		std::vector<VkDescriptorSet> sky;
	} descriptor_sets;

	RenderTarget render_target;
	VkRenderPass render_pass;

public:
	RenderTaskGBuffer(Scene & scene) : scene(scene) { }

	void init(VkDescriptorPool descriptor_pool, int width, int height, int swapchain_image_count);
	void free();

	void render(int image_index, VkCommandBuffer command_buffer);

	uint32_t get_num_descriptor_sets(uint32_t swapchain_image_count) {
		return swapchain_image_count * 4 + scene.asset_manager.textures.size();
	}

	RenderTarget const & get_render_target() { return render_target; }
};
