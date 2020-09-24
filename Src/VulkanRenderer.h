#pragma once
#include <vector>

#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

#include "Camera.h"
#include "Mesh.h"

#include "Types.h"

class VulkanRenderer {
	u32 width;
	u32 height;
	
	GLFWwindow * window;

	VkSwapchainKHR           swapchain;
	std::vector<VkImageView> image_views;
	
	VkRenderPass     render_pass;
	VkPipelineLayout pipeline_layout;
	VkPipeline       pipeline;

	VkDescriptorSetLayout descriptor_set_layout;

	std::vector<VkFramebuffer> framebuffers;

	std::vector<VkCommandBuffer> command_buffers;

	VkImage        depth_image;
	VkDeviceMemory depth_image_memory;
	VkImageView    depth_image_view;

	VkBuffer       vertex_buffer;
	VkDeviceMemory vertex_buffer_memory;

	VkBuffer       index_buffer;
	VkDeviceMemory index_buffer_memory;

	std::vector<VkBuffer>       uniform_buffers;
	std::vector<VkDeviceMemory> uniform_buffers_memory;

	VkDescriptorPool             descriptor_pool;
	std::vector<VkDescriptorSet> descriptor_sets;

	VkImage        texture_image;
	VkDeviceMemory texture_image_memory;
	VkImageView    texture_image_view;
	VkSampler      texture_sampler;

	std::vector<VkSemaphore> semaphores_image_available;
	std::vector<VkSemaphore> semaphores_render_done;
	
	std::vector<VkFence> inflight_fences;
	std::vector<VkFence> images_in_flight;
	
	int current_frame = 0;

	static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
	
	Mesh const * mesh;

	void create_swapchain();

	void create_descriptor_set_layout();
	void create_pipeline();
	void create_depth_buffer();
	void create_framebuffers();
	void create_vertex_buffer();
	void create_index_buffer();
	void create_texture();
	void create_uniform_buffers();
	void create_descriptor_pool();
	void create_descriptor_sets();
	void create_command_buffers();
	void create_sync_primitives();

	void destroy_swapchain();

public:
	bool framebuffer_needs_resize;

	Camera camera;
	
	VulkanRenderer(GLFWwindow * window, u32 width, u32 height);
	~VulkanRenderer();

	void render();
};
