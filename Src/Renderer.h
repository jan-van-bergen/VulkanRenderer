#pragma once
#include <vector>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "RenderTaskGbuffer.h"
#include "RenderTaskShadow.h"
#include "RenderTaskLighting.h"
#include "RenderTaskPostProcess.h"

class Renderer {
	GLFWwindow * window;

	VkSwapchainKHR           swapchain;
	std::vector<VkImageView> swapchain_views;

	std::vector<VkCommandBuffer> command_buffers;
	std::vector<VkFramebuffer>   frame_buffers;

	VkImage        depth_image;
	VkDeviceMemory depth_image_memory;
	VkImageView    depth_image_view;

	VkDescriptorPool descriptor_pool;

	static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

	VkSemaphore semaphores_image_available[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore semaphores_render_done    [MAX_FRAMES_IN_FLIGHT];

	VkFence              fences[MAX_FRAMES_IN_FLIGHT];
	std::vector<VkFence> fences_in_flight;

	RenderTaskGBuffer     render_task_gbuffer;
	RenderTaskShadow      render_task_shadow;
	RenderTaskLighting    render_task_lighting;
	RenderTaskPostProcess render_task_post_process;

	int current_frame = 0;

	struct {
		float frame_delta;
		float frame_avg;
		float frame_min;
		float frame_max;

		float time_since_last_second = 0.0f;
		int frames_since_last_second = 0;
		int fps = 0;

		int                frame_index = 0;
		std::vector<float> frame_times;
	} timing;

	void swapchain_create();
	void swapchain_destroy();

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
