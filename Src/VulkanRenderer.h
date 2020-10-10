#pragma once
#include <vector>

#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

#include "VulkanMemory.h"

#include "GBuffer.h"
#include "Lights.h"

class VulkanRenderer {
	GLFWwindow * window;

	VkSwapchainKHR           swapchain;
	std::vector<VkImageView> swapchain_views;

	struct {
		RenderTarget render_target_colour;
		RenderTarget render_target_depth;

		VkDescriptorSetLayout        descriptor_set_layout;
		std::vector<VkDescriptorSet> descriptor_sets;

		VkRenderPass render_pass;

		VkPipelineLayout pipeline_layout;
		VkPipeline       pipeline;

		VkFramebuffer frame_buffer;
		VkSampler sampler;
	} post_process;
	
	struct LightPass {
		VkPipelineLayout pipeline_layout;
		VkPipeline       pipeline;
	
		std::vector<VkDescriptorSet>      descriptor_sets;
		std::vector<VulkanMemory::Buffer> uniform_buffers;

		void free();
	};
	
	VkDescriptorSetLayout light_descriptor_set_layout;
	VkRenderPass          light_render_pass;
	
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
	
	VkFence fences_inflight[MAX_FRAMES_IN_FLIGHT];
	
	GBuffer   gbuffer;
	VkSampler gbuffer_sampler;

	int current_frame = 0;

	std::vector<MeshInstance> meshes;
	
	std::vector<DirectionalLight> directional_lights;
	std::vector<PointLight>       point_lights;
	std::vector<SpotLight>        spot_lights;

	// Timing
	float frame_delta;
	float frame_avg;
	float frame_min;
	float frame_max;

	float time_since_last_second = 0.0f;
	int frames_since_last_second = 0;
	int fps = 0;

	int frame_index = 0;
	std::vector<float> frame_times;

	void swapchain_create();
	void swapchain_destroy();

	void create_light_render_pass();

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

	void record_command_buffer(u32 image_index);

public:
	u32 width;
	u32 height;
	
	bool framebuffer_needs_resize;

	Camera camera;
	
	VulkanRenderer(GLFWwindow * window, u32 width, u32 height);
	~VulkanRenderer();

	void update(float delta);
	void render();
};
