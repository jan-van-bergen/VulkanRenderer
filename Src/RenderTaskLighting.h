#pragma once
#include <vector>
#include <string>

#include "VulkanMemory.h"

#include "Scene.h"
#include "RenderTarget.h"

struct RenderTaskLighting {
private:
	int width;
	int height;

	Scene & scene;

	struct LightPass {
		VkPipelineLayout pipeline_layout;
		VkPipeline       pipeline;
	
		std::vector<VkDescriptorSet>      descriptor_sets;
		std::vector<VulkanMemory::Buffer> uniform_buffers;

		void free();
	};

	VkDescriptorSetLayout light_descriptor_set_layout;
	VkDescriptorSetLayout shadow_descriptor_set_layout;
	
	RenderTarget render_target;
	VkRenderPass render_pass;

	LightPass light_pass_directional;
	LightPass light_pass_point;
	LightPass light_pass_spot;

	LightPass create_light_pass(
		VkDescriptorPool descriptor_pool,
		int width, int height, int swapchain_image_count,
		RenderTarget const & render_target_input,
		std::vector<VkVertexInputBindingDescription>   const & vertex_bindings,
		std::vector<VkVertexInputAttributeDescription> const & vertex_attributes,
		std::string const & filename_shader_vertex,
		std::string const & filename_shader_fragment,
		size_t push_constants_size,
		size_t ubo_size
	);

public:
	RenderTaskLighting(Scene & scene) : scene(scene) { }

	void init(VkDescriptorPool descriptor_pool, int width, int height, int swapchain_image_count, RenderTarget const & render_target_input);
	void free();

	void render(int image_index, VkCommandBuffer command_buffer);

	RenderTarget const & get_render_target() { return render_target; }
};
