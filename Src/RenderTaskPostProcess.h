#pragma once
#include <vector>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "Scene.h"
#include "Gizmo.h"

struct RenderTaskPostProcess {
private:
	int width;
	int height;

	Scene & scene;

	VkDescriptorPool descriptor_pool_gui;

	VkDescriptorSetLayout        descriptor_set_layout;
	std::vector<VkDescriptorSet> descriptor_sets;

	struct {
		VkPipelineLayout tonemap;
		VkPipelineLayout gizmo;
	} pipeline_layouts;

	struct {
		VkPipeline tonemap;
		VkPipeline gizmo;
	} pipelines;

	VkRenderPass render_pass;

public:
	Gizmo gizmo_position;
	Gizmo gizmo_rotation;
//	Gizmo gizmo_scale;

	RenderTaskPostProcess(Scene & scene);
	~RenderTaskPostProcess();

	void init(VkDescriptorPool descriptor_pool, int width, int height, int swapchain_image_count, RenderTarget const & render_target_input, GLFWwindow * window);
	void free();

	void render(int image_index, VkCommandBuffer command_buffer, VkFramebuffer frame_buffer);

	uint32_t get_num_descriptor_sets(uint32_t swapchain_image_count) {
		return swapchain_image_count;
	}

	VkRenderPass get_render_pass() { return render_pass; }
};
