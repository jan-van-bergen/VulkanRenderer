#pragma once
#include <vector>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "Scene.h"

struct RenderTaskPostProcess {
private:
	int width;
	int height;

	Scene & scene;
	
	VkDescriptorPool descriptor_pool_gui;

	VkDescriptorSetLayout        descriptor_set_layout;
	std::vector<VkDescriptorSet> descriptor_sets;

	VkPipelineLayout pipeline_layout;
	VkPipeline       pipeline;

	VkRenderPass render_pass;

public:
	RenderTaskPostProcess(Scene & scene);
	~RenderTaskPostProcess();

	void init(VkDescriptorPool descriptor_pool, int width, int height, int swapchain_image_count, RenderTarget const & render_target_input, GLFWwindow * window);
	void free();

	void render(int image_index, VkCommandBuffer command_buffer, VkFramebuffer frame_buffer);

	VkRenderPass get_render_pass() { return render_pass; }
};
