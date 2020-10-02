#pragma once
#include <vector>

#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

#include "VulkanMemory.h"

#include "Camera.h"
#include "GBuffer.h"

#include "Renderable.h"

class VulkanRenderer {
	GLFWwindow * window;

	VkSwapchainKHR           swapchain;
	std::vector<VkImageView> image_views;
	
	VkRenderPass render_pass;
	
	VkPipelineLayout pipeline_layout;
	VkPipeline       pipeline;
	
	VkDescriptorSetLayout descriptor_set_layout;

	std::vector<VkFramebuffer> frame_buffers;

	std::vector<VkCommandBuffer> command_buffers;

	VkImage        depth_image;
	VkDeviceMemory depth_image_memory;
	VkImageView    depth_image_view;

	VkDescriptorPool             descriptor_pool;
	std::vector<VkDescriptorSet> descriptor_sets;

	VkDescriptorPool descriptor_pool_gui;

	std::vector<VkSemaphore> semaphores_image_available;
	std::vector<VkSemaphore> semaphores_gbuffer_done;
	std::vector<VkSemaphore> semaphores_render_done;
	
	std::vector<VkFence> inflight_fences;
	std::vector<VkFence> images_in_flight;
	
	GBuffer   gbuffer;
	VkSampler gbuffer_sampler;

	int current_frame = 0;

	static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
	
	std::vector<Renderable> renderables;
	std::vector<Texture *>  textures;

	// Timing
	float frame_delta;
	float time_since_last_second = 0.0f;
	int frames_since_last_second = 0;
	int fps = 0;

	void swapchain_create();
	void swapchain_destroy();

	void create_descriptor_set_layout();
	void create_pipeline();
	void create_depth_buffer();
	void create_frame_buffers();
	void create_descriptor_sets();
	void create_command_buffers();
	void create_sync_primitives();
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
