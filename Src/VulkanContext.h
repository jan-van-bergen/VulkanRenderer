#pragma once
#include <vector>
#include <optional>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

#include "Types.h"

namespace VulkanContext {
	inline VkSurfaceFormatKHR const FORMAT = { VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	inline VkPresentModeKHR   const PRESENT_MODE = VK_PRESENT_MODE_MAILBOX_KHR;
	
	inline VkFormat const DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;

	void init(GLFWwindow * window);
	void destroy();

	VkSwapchainKHR create_swapchain(u32 width, u32 height);

	VkInstance get_instance();

	VkPhysicalDevice get_physical_device();
	VkDevice         get_device();

	VkSurfaceKHR get_surface();

	u32 get_queue_family_graphics();
	u32 get_queue_family_present();

	VkQueue get_queue_graphics();
	VkQueue get_queue_present();

	VkCommandPool get_command_pool();
};
