#pragma once
#include <vulkan/vulkan.h>

#include "Scene.h"

struct RenderTaskShadow {
private:
	Scene & scene;

	inline static constexpr int SHADOW_MAP_WIDTH  = 2048;
	inline static constexpr int SHADOW_MAP_HEIGHT = 2048;

	VkDescriptorSetLayout descriptor_set_layout;

	VkPipelineLayout pipeline_layout;
	VkPipeline       pipeline;
		
	VkRenderPass render_pass;

public:
	RenderTaskShadow(Scene & scene) : scene(scene) { }

	void init();
	void free();

	void render(int image_index, VkCommandBuffer command_buffer);
};
