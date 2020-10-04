#include "VulkanCheck.h"
#include "VulkanContext.h"
#include "VulkanMemory.h"
#include "VulkanRenderer.h"

#include "Vector2.h"
#include "Vector3.h"
#include "Matrix4.h"

#include "Input.h"
#include "Camera.h"

static u32 screen_width  = 1280;
static u32 screen_height = 720;

static void glfw_framebuffer_resize_callback(GLFWwindow * window, int width, int height) {
	reinterpret_cast<VulkanRenderer *>(glfwGetWindowUserPointer(window))->framebuffer_needs_resize = true;
}

int main() {
	int init_result = glfwInit();
	if (init_result == GLFW_FALSE) {
		puts("ERROR: Unable to create GLFW context!");
		abort();
	}

	// Init GLFW window
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	GLFWwindow * window = glfwCreateWindow(screen_width, screen_height, "Vulkan", nullptr, nullptr);

	Input::detail::init(window);

	glfwSetFramebufferSizeCallback(window, &glfw_framebuffer_resize_callback);
	glfwSetKeyCallback            (window, &Input::detail::glfw_callback_key);
	glfwSetCursorPosCallback      (window, &Input::detail::glfw_callback_mouse);

	std::vector<char const *> device_extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	VulkanContext::init(window);
	{
		VulkanRenderer renderer(window, screen_width, screen_height);
		renderer.camera.position.z = 12.0f;
	
		glfwSetWindowUserPointer(window, &renderer);
		
		double time_curr = 0.0f;
		double time_prev = 0.0f;
		double time_delta;

		// Main loop
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();

			time_curr  = glfwGetTime();
			time_delta = time_curr - time_prev;
			time_prev  = time_curr;

			renderer.update(float(time_delta));
			renderer.render();

			Input::detail::finish_frame();
		}

		// Sync before destroying
		VK_CHECK(vkDeviceWaitIdle(VulkanContext::get_device()));
	}
	VulkanContext::destroy();
	
	glfwDestroyWindow(window);
	glfwTerminate();

	puts("Press any key to continue..."); getchar();

	return 0;
}
