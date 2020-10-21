#pragma once
#include <vulkan/vulkan.h>

#include "Scene.h"

struct RenderTaskShadow {
private:
	Scene & scene;

	inline static constexpr int SHADOW_MAP_WIDTH  = 2048;
	inline static constexpr int SHADOW_MAP_HEIGHT = 2048;

	struct {
		VkDescriptorSetLayout shadow_static;
		VkDescriptorSetLayout shadow_animated;
	} descriptor_set_layouts;
	
	struct {
		VkPipelineLayout shadow_static;
		VkPipelineLayout shadow_animated;
	} pipeline_layouts;

	struct {
		VkPipeline shadow_static;
		VkPipeline shadow_animated;
	} pipelines;
		
	struct {
		std::vector<VkDescriptorSet> bones;
	} descriptor_sets;

	VkRenderPass render_pass;

public:
	RenderTaskShadow(Scene & scene) : scene(scene) { }

	void init(VkDescriptorPool descriptor_pool, int swapchain_image_count);
	void free();

	void render(int image_index, VkCommandBuffer command_buffer);
};
