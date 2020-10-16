#pragma once
#include <vector>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "VulkanMemory.h"

#include "Scene.h"

class Renderer {
	GLFWwindow * window;

	VkSwapchainKHR           swapchain;
	std::vector<VkImageView> swapchain_views;

	struct {	
		struct {
			VkPipeline geometry;
			VkPipeline sky;
		} pipelines;

		struct {
			VkPipelineLayout geometry;
			VkPipelineLayout sky;
		} pipeline_layouts;

		struct {
			VkDescriptorSetLayout geometry;
			VkDescriptorSetLayout sky;
		} descriptor_set_layouts;
	
		std::vector<VulkanMemory::Buffer> uniform_buffers;
		std::vector<VkDescriptorSet>      descriptor_sets_sky;

		RenderTarget render_target;
		VkRenderPass render_pass;
	} gbuffer;

	struct {
		inline static constexpr int WIDTH  = 2048;
		inline static constexpr int HEIGHT = 2048;

		VkDescriptorSetLayout descriptor_set_layout;

		VkPipelineLayout pipeline_layout;
		VkPipeline       pipeline;
		
		VkRenderPass render_pass;
	} shadow;

	struct {
		VkDescriptorSetLayout        descriptor_set_layout;
		std::vector<VkDescriptorSet> descriptor_sets_sky;

		VkPipelineLayout pipeline_layout;
		VkPipeline       pipeline;
		
		VkRenderPass render_pass;
	} post_process;
	
	struct LightPass {
		VkPipelineLayout pipeline_layout;
		VkPipeline       pipeline;
	
		std::vector<VkDescriptorSet>      descriptor_sets;
		std::vector<VulkanMemory::Buffer> uniform_buffers;

		void free();
	};
	
	VkDescriptorSetLayout light_descriptor_set_layout;
	VkDescriptorSetLayout shadow_descriptor_set_layout;
	
	RenderTarget light_render_target;
	VkRenderPass light_render_pass;

	LightPass light_pass_directional;
	LightPass light_pass_point;
	LightPass light_pass_spot;
	
	std::vector<VkCommandBuffer> command_buffers;
	std::vector<VkFramebuffer>   frame_buffers;

	VkImage        depth_image;
	VkDeviceMemory depth_image_memory;
	VkImageView    depth_image_view;

	VkDescriptorPool descriptor_pool;
	VkDescriptorPool descriptor_pool_gui;
	
	static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
	
	VkSemaphore semaphores_image_available[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore semaphores_gbuffer_done   [MAX_FRAMES_IN_FLIGHT];
	VkSemaphore semaphores_render_done    [MAX_FRAMES_IN_FLIGHT];
	
	VkFence              fences[MAX_FRAMES_IN_FLIGHT];
	std::vector<VkFence> fences_in_flight;

	int current_frame = 0;

	// Timing
	float frame_delta;
	float frame_avg;
	float frame_min;
	float frame_max;

	float time_since_last_second = 0.0f;
	int frames_since_last_second = 0;
	int fps = 0;

	int                frame_index = 0;
	std::vector<float> frame_times;

	void swapchain_create();
	void swapchain_destroy();

	void create_descriptor_pool();

	void create_gbuffer();
	void create_shadow_render_pass();
	
	LightPass create_light_pass(
		std::vector<VkVertexInputBindingDescription>   const & vertex_bindings,
		std::vector<VkVertexInputAttributeDescription> const & vertex_attributes,
		std::string const & filename_shader_vertex,
		std::string const & filename_shader_fragment,
		size_t push_constants_size,
		size_t ubo_size
	);

	void create_post_process();

	void create_frame_buffers();
	void create_command_buffers();
	void create_imgui();

	void record_command_buffer_gbuffer(u32 image_index);
	void record_command_buffer(u32 image_index);

public:
	u32 width;
	u32 height;
	
	bool framebuffer_needs_resize;
	
	Scene scene;

	Renderer(GLFWwindow * window, u32 width, u32 height);
	~Renderer();

	void update(float delta);
	void render();
};
