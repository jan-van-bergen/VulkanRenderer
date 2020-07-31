#include "VulkanContext.h"

#include <set>

#include "VulkanCall.h"
#include "VulkanMemory.h"

#include "Math.h"

static VkInstance instance;

static VkDebugUtilsMessengerEXT debug_messenger = nullptr;
static VkPhysicalDevice physical_device;

static VkDevice device;

static VkSurfaceKHR surface;

static u32 queue_family_graphics;
static u32 queue_family_present;

static VkQueue queue_graphics;
static VkQueue queue_present;

static VkCommandPool command_pool;

#ifdef NDEBUG
bool validation_layers_enabled = false;
#else
bool validation_layers_enabled = true;
#endif

std::vector<char const *> const validation_layers_names = {
	"VK_LAYER_KHRONOS_validation",
	//"VK_LAYER_RENDERDOC_Capture"
};

static VkBool32 debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT       msg_severity,
	VkDebugUtilsMessageTypeFlagsEXT              msg_type,
	VkDebugUtilsMessengerCallbackDataEXT const * callback_data,
	void                                       * user_data
) {
	printf("VULKAN Debug Callback: '%s'\n", callback_data->pMessage);
	__debugbreak();

	return VK_FALSE;
}

std::vector<char const *> const device_extensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#define VULKAN_PROC(func_name) ( (PFN_##func_name)vkGetInstanceProcAddr(instance, #func_name) )

static void init_instance() {
	// Get GLFW required extensions
	u32           glfw_extension_count = 0;
	char const ** glfw_extensions(glfwGetRequiredInstanceExtensions(&glfw_extension_count));
	
	std::vector<char const *> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

	// Init validation layers
	if (validation_layers_enabled) {
		u32 layer_count;
		vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

		std::vector<VkLayerProperties> layers(layer_count);
		vkEnumerateInstanceLayerProperties(&layer_count, layers.data());

		for (char const * layer_name : validation_layers_names) {
			bool available = false;

			for (VkLayerProperties const & layer : layers) {
				if (strcmp(layer_name, layer.layerName) == 0) {
					available = true;
					break;
				}
			}

			if (!available) __debugbreak();
		}

		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); // Additional debug extension
	}
	
	VkApplicationInfo app_info = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app_info.pApplicationName = "Hello Triangle";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "Vulkan";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo instance_create_info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	instance_create_info.pApplicationInfo = &app_info;
	instance_create_info.enabledLayerCount   = validation_layers_names.size();
	instance_create_info.ppEnabledLayerNames = validation_layers_names.data();
	instance_create_info.enabledExtensionCount   = extensions.size();
	instance_create_info.ppEnabledExtensionNames = extensions.data();
	
	VkDebugUtilsMessengerCreateInfoEXT callback_create_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };

	if (validation_layers_enabled) {		
		callback_create_info.messageSeverity =
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		callback_create_info.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		callback_create_info.pfnUserCallback = debug_callback;
		callback_create_info.pUserData = nullptr;

		instance_create_info.pNext = &callback_create_info;
	}
	
	VULKAN_CALL(vkCreateInstance(&instance_create_info, nullptr, &instance));
	
	if (validation_layers_enabled) {
		VULKAN_PROC(vkCreateDebugUtilsMessengerEXT)(instance, &callback_create_info, nullptr, &debug_messenger);
	}
}

static void init_physical_device() {
	u32 device_count = 0;
	vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
	
	if (device_count == 0) {
		printf("ERROR: No Physical Devices available!\n");
		abort();
	}

	std::vector<VkPhysicalDevice> devices(device_count);
	vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
	
	physical_device = devices[0];
}

static void init_surface(GLFWwindow * window) {
	VULKAN_CALL(glfwCreateWindowSurface(instance, window, nullptr, &surface));	
}

static void init_queue_families() {
	u32 queue_families_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, nullptr);

	std::vector<VkQueueFamilyProperties> queue_families(queue_families_count);
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, queue_families.data());
	
	std::optional<u32> opt_queue_family_graphics;
	std::optional<u32> opt_queue_family_present;

	for (int i = 0; i < queue_families_count; i++) {
		VkBool32 supports_graphics = queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
		VkBool32 supports_present  = false;
		VULKAN_CALL(vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &supports_present));

		if (supports_graphics) opt_queue_family_graphics = i;
		if (supports_present)  queue_family_present  = i;

		if (opt_queue_family_graphics.has_value() && opt_queue_family_present.has_value()) break;
	}

	if (!opt_queue_family_graphics.has_value() && !opt_queue_family_present.has_value()) {
		printf("Failed to create queue families!\n");	
		abort();
	}

	queue_family_graphics = opt_queue_family_graphics.value();
	queue_family_present  = opt_queue_family_present .value();
}

static void init_device() {
	float queue_priority = 1.0f;
	
	std::set<u32> unique_queue_families = {
		queue_family_graphics,
		queue_family_present
	};
	std::vector<VkDeviceQueueCreateInfo> queue_create_infos;

	for (u32 queue_family : unique_queue_families) {
		VkDeviceQueueCreateInfo queue_create_info = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
		queue_create_info.queueFamilyIndex = queue_family;
		queue_create_info.queueCount       = 1;
		queue_create_info.pQueuePriorities = &queue_priority;
		queue_create_infos.push_back(queue_create_info);
	}
	
	VkPhysicalDeviceFeatures device_features = { };
	device_features.samplerAnisotropy = VK_TRUE;

	VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures dsf = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES };

	VkPhysicalDeviceFeatures2 query = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
	query.pNext = &dsf;
	vkGetPhysicalDeviceFeatures2(physical_device, &query);

	VkDeviceCreateInfo device_create_info = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	device_create_info.pQueueCreateInfos    = queue_create_infos.data();
	device_create_info.queueCreateInfoCount = queue_create_infos.size();
	device_create_info.pEnabledFeatures = &device_features;
	device_create_info.enabledExtensionCount   = device_extensions.size();
	device_create_info.ppEnabledExtensionNames = device_extensions.data();
	device_create_info.pNext = &dsf;

	if (validation_layers_enabled) {
		device_create_info.enabledLayerCount   = validation_layers_names.size();
		device_create_info.ppEnabledLayerNames = validation_layers_names.data();
	} else {
		device_create_info.enabledLayerCount = 0;
	}
	
	VULKAN_CALL(vkCreateDevice(physical_device, &device_create_info, nullptr, &device));
}

static void init_queues() {
	vkGetDeviceQueue(device, queue_family_graphics, 0, &queue_graphics);
	vkGetDeviceQueue(device, queue_family_present,  0, &queue_present);
}

static void init_command_pool() {
	VkCommandPoolCreateInfo command_pool_create_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	command_pool_create_info.queueFamilyIndex = queue_family_graphics;
	command_pool_create_info.flags = 0;

	VULKAN_CALL(vkCreateCommandPool(device, &command_pool_create_info, nullptr, &command_pool));
}

void VulkanContext::init(GLFWwindow * window) {
	init_instance();
	init_physical_device();
	init_surface(window);
	init_queue_families();
	init_device();
	init_queues();
	init_command_pool();
}

void VulkanContext::destroy() {
	vkDestroyCommandPool(device, command_pool, nullptr);

	if (validation_layers_enabled) {
		VULKAN_PROC(vkDestroyDebugUtilsMessengerEXT)(instance, debug_messenger, nullptr);
	}

	vkDestroySurfaceKHR(instance, surface, nullptr);
	
	vkDestroyDevice  (device,   nullptr);
	vkDestroyInstance(instance, nullptr);
}

VkSwapchainKHR VulkanContext::create_swapchain(u32 width, u32 height) {
	VkSurfaceCapabilitiesKHR surface_capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities);

	VkExtent2D extent;
	extent.width  = Math::max(surface_capabilities.minImageExtent.width,  Math::min(surface_capabilities.maxImageExtent.width , width));
	extent.height = Math::max(surface_capabilities.minImageExtent.height, Math::min(surface_capabilities.maxImageExtent.height, height));

	u32 image_count = surface_capabilities.minImageCount + 1;
	if (surface_capabilities.maxImageCount > 0 && image_count > surface_capabilities.maxImageCount) {
		image_count = surface_capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapchain_create_info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	swapchain_create_info.surface = surface;
	swapchain_create_info.minImageCount = image_count;
	swapchain_create_info.imageFormat     = FORMAT.format;
	swapchain_create_info.imageColorSpace = FORMAT.colorSpace;
	swapchain_create_info.imageExtent = extent;
	swapchain_create_info.imageArrayLayers = 1;
	swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	u32 queue_family_indices[] = {
		queue_family_graphics,
		queue_family_present
	};

	if (queue_family_graphics != queue_family_present) {
		swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchain_create_info.queueFamilyIndexCount = 2;
		swapchain_create_info.pQueueFamilyIndices   = queue_family_indices;
	} else {
		swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchain_create_info.queueFamilyIndexCount = 0;
		swapchain_create_info.pQueueFamilyIndices   = nullptr;
	}

	swapchain_create_info.preTransform = surface_capabilities.currentTransform;
	swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchain_create_info.presentMode = PRESENT_MODE;
	swapchain_create_info.clipped = VK_TRUE;
	swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

	VkSwapchainKHR swapchain;
	VULKAN_CALL(vkCreateSwapchainKHR(device, &swapchain_create_info, nullptr, &swapchain));

	return swapchain;
}

VkPhysicalDevice VulkanContext::get_physical_device() { return physical_device; }
VkDevice         VulkanContext::get_device()          { return device; }

VkSurfaceKHR VulkanContext::get_surface() { return surface; }

VkQueue VulkanContext::get_queue_graphics() { return queue_graphics; }
VkQueue VulkanContext::get_queue_present()  { return queue_present;  }

VkCommandPool VulkanContext::get_command_pool() { return command_pool; }
