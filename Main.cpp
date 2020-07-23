#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#include <vector>
#include <optional>
#include <set>

#include <string>

#include <chrono>
#include <algorithm>

#include "Util.h"
#include "Types.h"
#include "VulkanCall.h"

#include "Vector2.h"
#include "Vector3.h"
#include "Matrix4.h"

#include "Input.h"
#include "Camera.h"

static u32 screen_width  = 900;
static u32 screen_height = 600;

static VkExtent2D extent = { screen_width, screen_height };

static GLFWwindow * window;

static bool framebuffer_needs_resize = false;

static VkSurfaceFormatKHR const format = { VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
static VkPresentModeKHR   const present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
	
static VkFormat const DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;

std::vector<char const *> const validation_layers_names = {
	"VK_LAYER_KHRONOS_validation",
	//"VK_LAYER_RENDERDOC_Capture"
};

#ifdef NDEBUG
bool validation_layers_enabled = false;
#else
bool validation_layers_enabled = true;
#endif

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

#define VULKAN_PROC(func_name) ( (PFN_##func_name)vkGetInstanceProcAddr(instance, #func_name) )

std::vector<char const *> const device_extensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

static VkInstance instance;

static VkDebugUtilsMessengerEXT debug_messenger = nullptr;
static VkPhysicalDevice physical_device;

static VkSurfaceKHR surface;

static std::optional<u32> queue_family_graphics;
static std::optional<u32> queue_family_present;

static VkDevice device;

static VkQueue queue_graphics;
static VkQueue queue_present;

static VkSwapchainKHR swapchain;

static std::vector<VkImageView> image_views;

static VkRenderPass     render_pass;
static VkPipelineLayout pipeline_layout;
static VkPipeline       pipeline;

static VkDescriptorSetLayout descriptor_set_layout;

static std::vector<VkFramebuffer> framebuffers;

static VkCommandPool                command_pool;
static std::vector<VkCommandBuffer> command_buffers;

static VkImage        depth_image;
static VkDeviceMemory depth_image_memory;
static VkImageView    depth_image_view;

static VkBuffer       vertex_buffer;
static VkDeviceMemory vertex_buffer_memory;

static VkBuffer       index_buffer;
static VkDeviceMemory index_buffer_memory;

static std::vector<VkBuffer>       uniform_buffers;
static std::vector<VkDeviceMemory> uniform_buffers_memory;

static VkDescriptorPool             descriptor_pool;
static std::vector<VkDescriptorSet> descriptor_sets;

static VkImage        texture_image;
static VkDeviceMemory texture_image_memory;
static VkImageView    texture_image_view;
static VkSampler      texture_sampler;

static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
static std::vector<VkSemaphore> semaphores_image_available(MAX_FRAMES_IN_FLIGHT);
static std::vector<VkSemaphore> semaphores_render_done    (MAX_FRAMES_IN_FLIGHT);
	
static std::vector<VkFence> inflight_fences(MAX_FRAMES_IN_FLIGHT);
static std::vector<VkFence> images_in_flight;

struct Vertex {
	Vector3 position;
	Vector2 texcoord;
	Vector3 colour;

	static VkVertexInputBindingDescription get_binding_description() {
		VkVertexInputBindingDescription bindingDescription = { };
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static std::vector<VkVertexInputAttributeDescription> get_attribute_description() {
		std::vector<VkVertexInputAttributeDescription> attribute_descriptions(3);

		// Position
		attribute_descriptions[0].binding  = 0;
		attribute_descriptions[0].location = 0;
		attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attribute_descriptions[0].offset = offsetof(Vertex, position);
		
		// Texture Coordinates
		attribute_descriptions[1].binding  = 0;
		attribute_descriptions[1].location = 1;
		attribute_descriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
		attribute_descriptions[1].offset = offsetof(Vertex, texcoord);

		// Colour
		attribute_descriptions[2].binding  = 0;
		attribute_descriptions[2].location = 2;
		attribute_descriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
		attribute_descriptions[2].offset = offsetof(Vertex, colour);

		return attribute_descriptions;
	}
};

static std::vector<Vertex> const vertices = {
	{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
	{ {  0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
	{ {  0.5f,  0.5f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
	{ { -0.5f,  0.5f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } },

	{ { -0.5f, -0.5f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
	{ {  0.5f, -0.5f, -1.0f }, { 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
	{ {  0.5f,  0.5f, -1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
	{ { -0.5f,  0.5f, -1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } }
};

static std::vector<u32> const indices = {
	0, 1, 2, 2, 3, 0,
	4, 5, 6, 6, 7, 4
};

struct UniformBufferObject {
	alignas(16) Matrix4 wvp;
};

static Camera camera(DEG_TO_RAD(110.0f), screen_width, screen_height);

static u32 find_memory_type(u32 type_filter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memory_properties;
	vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

	for (u32 i = 0; i < memory_properties.memoryTypeCount; i++) {
		if ((type_filter & (1 << i)) && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	abort();
}

static void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& buffer_memory) {
	VkBufferCreateInfo buffer_create_info = { };
	buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_create_info.size = size;
	buffer_create_info.usage = usage;
	buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VULKAN_CALL(vkCreateBuffer(device, &buffer_create_info, nullptr, &buffer));

	VkMemoryRequirements requirements;
	vkGetBufferMemoryRequirements(device, buffer, &requirements);

	VkMemoryAllocateInfo alloc_info = { };
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = requirements.size;
	alloc_info.memoryTypeIndex = find_memory_type(requirements.memoryTypeBits, properties);

	VULKAN_CALL(vkAllocateMemory(device, &alloc_info, nullptr, &buffer_memory));

	VULKAN_CALL(vkBindBufferMemory(device, buffer, buffer_memory, 0));
}

static VkCommandBuffer command_buffer_single_use_begin() {
	VkCommandBufferAllocateInfo alloc_info = { };
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandPool = command_pool;
	alloc_info.commandBufferCount = 1;

	VkCommandBuffer command_buffer;
	VULKAN_CALL(vkAllocateCommandBuffers(device, &alloc_info, &command_buffer));

	VkCommandBufferBeginInfo begin_info = { };
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VULKAN_CALL(vkBeginCommandBuffer(command_buffer, &begin_info));

	return command_buffer;
}

static void command_buffer_single_use_end(VkCommandBuffer command_buffer) {
	vkEndCommandBuffer(command_buffer);

	VkSubmitInfo submit_info = { };
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;

	VULKAN_CALL(vkQueueSubmit(queue_graphics, 1, &submit_info, VK_NULL_HANDLE));
	VULKAN_CALL(vkQueueWaitIdle(queue_graphics));

	vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
}

static void buffer_copy(VkBuffer buffer_dst, VkBuffer buffer_src, VkDeviceSize size) {
	VkCommandBuffer copy_command_buffer = command_buffer_single_use_begin();

	VkBufferCopy buffer_copy = { };
	buffer_copy.srcOffset = 0;
	buffer_copy.dstOffset = 0;
	buffer_copy.size = size;
	vkCmdCopyBuffer(copy_command_buffer, buffer_src, buffer_dst, 1, &buffer_copy);

	command_buffer_single_use_end(copy_command_buffer);
}

static void buffer_memory_copy(VkDeviceMemory device_memory, void const * data, u64 size) {
	void * dst;
	vkMapMemory(device, device_memory, 0, size, 0, &dst);

	memcpy(dst, data, size);

	vkUnmapMemory(device, device_memory);
}

static void create_image(u32 width, u32 height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage & image, VkDeviceMemory & image_memory) {
	VkImageCreateInfo image_create_info = { };
	image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_create_info.imageType = VK_IMAGE_TYPE_2D;
	image_create_info.extent.width  = width;
	image_create_info.extent.height = height;
	image_create_info.extent.depth  = 1;
	image_create_info.mipLevels   = 1;
	image_create_info.arrayLayers = 1;
	image_create_info.format = format;
	image_create_info.tiling = tiling;
	image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_create_info.usage         = usage;
	image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_create_info.flags = 0;

	VULKAN_CALL(vkCreateImage(device, &image_create_info, nullptr, &image));

	VkMemoryRequirements requirements;
	vkGetImageMemoryRequirements(device, image, &requirements);

	VkMemoryAllocateInfo alloc_info = { };
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = requirements.size;
	alloc_info.memoryTypeIndex = find_memory_type(requirements.memoryTypeBits, properties);

	VULKAN_CALL(vkAllocateMemory(device, &alloc_info, nullptr, &image_memory));

	VULKAN_CALL(vkBindImageMemory(device, image, image_memory, 0));
}

static VkImageView create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect_mask) {
	VkImageViewCreateInfo image_view_create_info = { };
	image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	image_view_create_info.image = image;
	image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	image_view_create_info.format = format;
	
	image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	image_view_create_info.subresourceRange.aspectMask = aspect_mask;
	image_view_create_info.subresourceRange.baseMipLevel = 0;
	image_view_create_info.subresourceRange.levelCount   = 1;
	image_view_create_info.subresourceRange.baseArrayLayer = 0;
	image_view_create_info.subresourceRange.layerCount     = 1;

	VkImageView image_view;
	VULKAN_CALL(vkCreateImageView(device, &image_view_create_info, nullptr, &image_view));

	return image_view;
}

void transition_image_layout(VkImage image, VkFormat format, VkImageLayout layout_old, VkImageLayout layout_new) {
	VkCommandBuffer command_buffer = command_buffer_single_use_begin();

	VkImageMemoryBarrier barrier = { };
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = layout_old;
	barrier.newLayout = layout_new;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount   = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount     = 1;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = 0;

	VkPipelineStageFlags stage_src;
	VkPipelineStageFlags stage_dst;

	if (layout_old == VK_IMAGE_LAYOUT_UNDEFINED && layout_new == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		stage_src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		stage_dst = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if (layout_old == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && layout_new == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		stage_src = VK_PIPELINE_STAGE_TRANSFER_BIT;
		stage_dst = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else if (layout_old == VK_IMAGE_LAYOUT_UNDEFINED && layout_new == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		stage_src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		stage_dst = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	} else {
		printf("ERROR: unsupported layout transition!");
		abort();
	}

	vkCmdPipelineBarrier(command_buffer, stage_src, stage_dst, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	command_buffer_single_use_end(command_buffer);
}

static void buffer_copy_to_image(VkBuffer buffer, VkImage image, u32 width, u32 height) {
	VkCommandBuffer command_buffer = command_buffer_single_use_begin();

	VkBufferImageCopy region = { };
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount     = 1;

	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = { width, height, 1 };

	vkCmdCopyBufferToImage(
		command_buffer,
		buffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region
	);

	command_buffer_single_use_end(command_buffer);
}


static void glfw_framebuffer_resize_callback(GLFWwindow * window, int width, int height) {
	framebuffer_needs_resize = true;
}

static void glfw_key_callback(GLFWwindow * window, int key, int scancode, int action, int mods) {
	Input::update_key(key, action);
}

static void init_glfw() {
	glfwInit();

	// Init GLFW window
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	window = glfwCreateWindow(screen_width, screen_height, "Vulkan", nullptr, nullptr);

	glfwSetFramebufferSizeCallback(window, glfw_framebuffer_resize_callback);
	glfwSetKeyCallback            (window, glfw_key_callback);
}

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
	
	VkApplicationInfo app_info = { };
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "Hello Triangle";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "Vulkan";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo instance_create_info = { };
	instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instance_create_info.pApplicationInfo = &app_info;
	instance_create_info.enabledLayerCount   = validation_layers_names.size();
	instance_create_info.ppEnabledLayerNames = validation_layers_names.data();
	instance_create_info.enabledExtensionCount   = extensions.size();
	instance_create_info.ppEnabledExtensionNames = extensions.data();
	
	VkDebugUtilsMessengerCreateInfoEXT callback_create_info = { };

	if (validation_layers_enabled) {		
		callback_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
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

static void init_surface() {
	VULKAN_CALL(glfwCreateWindowSurface(instance, window, nullptr, &surface));	
}

static void init_queue_families() {
	u32 queue_families_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, nullptr);

	std::vector<VkQueueFamilyProperties> queue_families(queue_families_count);
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, queue_families.data());
	
	for (int i = 0; i < queue_families_count; i++) {
		VkBool32 supports_graphics = queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
		VkBool32 supports_present  = false;
		VULKAN_CALL(vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &supports_present));

		if (supports_graphics) queue_family_graphics = i;
		if (supports_present)  queue_family_present  = i;

		if (queue_family_graphics.has_value() && queue_family_present.has_value()) break;
	}

	if (!queue_family_graphics.has_value() && !queue_family_present.has_value()) {
		printf("Failed to create queue families!\n");	
		abort();
	}
}

static void init_device() {
	float queue_priority = 1.0f;
	
	std::set<u32> unique_queue_families = {
		queue_family_graphics.value(),
		queue_family_present .value()
	};
	std::vector<VkDeviceQueueCreateInfo> queue_create_infos;

	for (u32 queue_family : unique_queue_families) {
		VkDeviceQueueCreateInfo queue_create_info = { };
		queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_create_info.queueFamilyIndex = queue_family;
		queue_create_info.queueCount       = 1;
		queue_create_info.pQueuePriorities = &queue_priority;
		queue_create_infos.push_back(queue_create_info);
	}
	
	VkPhysicalDeviceFeatures device_features = { };
	device_features.samplerAnisotropy = VK_TRUE;

	VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures dsf = { };
	dsf.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES;

	VkPhysicalDeviceFeatures2 query = { };
	query.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	query.pNext = &dsf;
	vkGetPhysicalDeviceFeatures2(physical_device, &query);

	VkDeviceCreateInfo device_create_info = { };
	device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
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
	vkGetDeviceQueue(device, queue_family_graphics.value(), 0, &queue_graphics);
	vkGetDeviceQueue(device, queue_family_present .value(), 0, &queue_present);
}

static void init_swapchain() {
	VkSurfaceCapabilitiesKHR surface_capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities);

	extent.width  = std::max(surface_capabilities.minImageExtent.width,  std::min(surface_capabilities.maxImageExtent.width , screen_width));
	extent.height = std::max(surface_capabilities.minImageExtent.height, std::min(surface_capabilities.maxImageExtent.height, screen_height));

	u32 image_count = surface_capabilities.minImageCount + 1;
	if (surface_capabilities.maxImageCount > 0 && image_count > surface_capabilities.maxImageCount) {
		image_count = surface_capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapchain_create_info = { };
	swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchain_create_info.surface = surface;
	swapchain_create_info.minImageCount = image_count;
	swapchain_create_info.imageFormat     = format.format;
	swapchain_create_info.imageColorSpace = format.colorSpace;
	swapchain_create_info.imageExtent = extent;
	swapchain_create_info.imageArrayLayers = 1;
	swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	u32 const queueFamilyIndices[] = {
		queue_family_graphics.value(),
		queue_family_present .value()
	};

	if (queue_family_graphics.value() != queue_family_present.value()) {
		swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchain_create_info.queueFamilyIndexCount = 2;
		swapchain_create_info.pQueueFamilyIndices   = queueFamilyIndices;
	} else {
		swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchain_create_info.queueFamilyIndexCount = 0;
		swapchain_create_info.pQueueFamilyIndices   = nullptr;
	}

	swapchain_create_info.preTransform = surface_capabilities.currentTransform;
	swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchain_create_info.presentMode = present_mode;
	swapchain_create_info.clipped = VK_TRUE;
	swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

	VULKAN_CALL(vkCreateSwapchainKHR(device, &swapchain_create_info, nullptr, &swapchain));

	u32 swapchain_image_count;
	vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, nullptr);

	std::vector<VkImage> swapchain_images(swapchain_image_count);
	vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, swapchain_images.data());

	image_views.resize(swapchain_image_count);

	for (int i = 0; i < swapchain_image_count; i++) {
		image_views[i] = create_image_view(swapchain_images[i], format.format, VK_IMAGE_ASPECT_COLOR_BIT);
	}
}

static VkShaderModule shader_load(VkDevice device, std::string const & filename) {
	std::vector<char> spirv = Util::read_file(filename);

	VkShaderModuleCreateInfo create_info = { };
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = spirv.size();
	create_info.pCode = reinterpret_cast<const u32 *>(spirv.data());

	VkShaderModule shader;
	VULKAN_CALL(vkCreateShaderModule(device, &create_info, nullptr, &shader));

	return shader;
}

static VkPipelineShaderStageCreateInfo shader_get_stage(VkShaderModule shader_module, VkShaderStageFlagBits stage) {
	VkPipelineShaderStageCreateInfo vertex_stage_create_info = { };
	vertex_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertex_stage_create_info.stage = stage;
	vertex_stage_create_info.module = shader_module;
	vertex_stage_create_info.pName = "main";

	return vertex_stage_create_info;
}

static void init_descriptor_set_layout() {
	VkDescriptorSetLayoutBinding layout_binding_ubo = { };
	layout_binding_ubo.binding = 0;
	layout_binding_ubo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	layout_binding_ubo.descriptorCount = 1;
	layout_binding_ubo.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	layout_binding_ubo.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding layout_binding_sampler = { };
	layout_binding_sampler.binding = 1;
	layout_binding_sampler.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	layout_binding_sampler.descriptorCount = 1;
	layout_binding_sampler.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	layout_binding_sampler.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding layout_bindings[] = {
		layout_binding_ubo,
		layout_binding_sampler
	};

	VkDescriptorSetLayoutCreateInfo layout_create_info = { };
	layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_create_info.bindingCount = Util::array_element_count(layout_bindings);
	layout_create_info.pBindings    = layout_bindings;

	VULKAN_CALL(vkCreateDescriptorSetLayout(device, &layout_create_info, nullptr, &descriptor_set_layout));
}

static void init_pipeline() {
	VkShaderModule shader_vert = shader_load(device, "Shaders\\vert.spv");
	VkShaderModule shader_frag = shader_load(device, "Shaders\\frag.spv");

	VkPipelineShaderStageCreateInfo shader_stages[] = {
		shader_get_stage(shader_vert, VK_SHADER_STAGE_VERTEX_BIT),
		shader_get_stage(shader_frag, VK_SHADER_STAGE_FRAGMENT_BIT),
	};

	VkVertexInputBindingDescription                binding_descriptions   = Vertex::get_binding_description();
	std::vector<VkVertexInputAttributeDescription> attribute_descriptions = Vertex::get_attribute_description();

	VkPipelineVertexInputStateCreateInfo vertex_input_create_info = { };
	vertex_input_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_create_info.vertexBindingDescriptionCount = 1;
	vertex_input_create_info.pVertexBindingDescriptions    = &binding_descriptions;
	vertex_input_create_info.vertexAttributeDescriptionCount = attribute_descriptions.size();
	vertex_input_create_info.pVertexAttributeDescriptions    = attribute_descriptions.data();

	VkPipelineInputAssemblyStateCreateInfo input_asm_create_info = { };
	input_asm_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_asm_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_asm_create_info.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = { };
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width  = static_cast<float>(extent.width);
	viewport.height = static_cast<float>(extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = { };
	scissor.offset = { 0, 0 };
	scissor.extent = extent;

	VkPipelineViewportStateCreateInfo viewport_state_create_info = { };
	viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state_create_info.viewportCount = 1;
	viewport_state_create_info.pViewports    = &viewport;
	viewport_state_create_info.scissorCount = 1;
	viewport_state_create_info.pScissors    = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer_create_info = { };
	rasterizer_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer_create_info.depthClampEnable = VK_FALSE;

	rasterizer_create_info.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer_create_info.lineWidth   = 1.0f;

	rasterizer_create_info.depthBiasEnable         = VK_FALSE;
	rasterizer_create_info.depthBiasConstantFactor = 0.0f;
	rasterizer_create_info.depthBiasClamp          = 0.0f;
	rasterizer_create_info.depthBiasSlopeFactor    = 0.0f;

	rasterizer_create_info.cullMode  = VK_CULL_MODE_BACK_BIT;
	rasterizer_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	VkPipelineMultisampleStateCreateInfo multisample_create_info = { };
	multisample_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_create_info.sampleShadingEnable = VK_FALSE;
	multisample_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample_create_info.minSampleShading = 1.0f;
	multisample_create_info.pSampleMask      = nullptr;
	multisample_create_info.alphaToCoverageEnable = VK_FALSE;
	multisample_create_info.alphaToOneEnable      = VK_FALSE;

	VkPipelineColorBlendAttachmentState blend = { };
	blend.blendEnable = VK_FALSE;
	blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blend.colorBlendOp = VK_BLEND_OP_ADD;
	blend.alphaBlendOp = VK_BLEND_OP_ADD;
	blend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blend.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

	VkPipelineColorBlendStateCreateInfo blend_state_create_info = { };
	blend_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state_create_info.logicOpEnable = VK_FALSE;
	blend_state_create_info.logicOp       = VK_LOGIC_OP_COPY;
	blend_state_create_info.attachmentCount = 1;
	blend_state_create_info.pAttachments    = &blend;
	blend_state_create_info.blendConstants[0] = 0.0f;
	blend_state_create_info.blendConstants[1] = 0.0f;
	blend_state_create_info.blendConstants[2] = 0.0f;
	blend_state_create_info.blendConstants[3] = 0.0f;

	VkPipelineLayoutCreateInfo pipeline_layout_create_info = { };
	pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_create_info.setLayoutCount = 1;
	pipeline_layout_create_info.pSetLayouts    = &descriptor_set_layout;
	pipeline_layout_create_info.pushConstantRangeCount = 0;
	pipeline_layout_create_info.pPushConstantRanges    = nullptr;

	VULKAN_CALL(vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &pipeline_layout));

	VkAttachmentDescription attachment_colour = { };
	attachment_colour.format = format.format;
	attachment_colour.samples = VK_SAMPLE_COUNT_1_BIT;
	attachment_colour.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment_colour.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment_colour.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment_colour.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment_colour.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachment_colour.finalLayout   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference attachment_ref_colour = { };
	attachment_ref_colour.attachment = 0;
	attachment_ref_colour.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription attachment_depth = { };
	attachment_depth.format = DEPTH_FORMAT;
	attachment_depth.samples = VK_SAMPLE_COUNT_1_BIT;
	attachment_depth.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment_depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment_depth.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment_depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment_depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachment_depth.finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	
	VkAttachmentReference attachment_ref_depth = { };
	attachment_ref_depth.attachment = 1;
	attachment_ref_depth.layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = { };
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount    = 1;
	subpass.pColorAttachments       = &attachment_ref_colour;
	subpass.pDepthStencilAttachment = &attachment_ref_depth;

	VkSubpassDependency dependency = { };
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkAttachmentDescription attachments[] = { attachment_colour, attachment_depth };

	VkRenderPassCreateInfo render_pass_create_info = { };
	render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_create_info.attachmentCount = Util::array_element_count(attachments);
	render_pass_create_info.pAttachments    = attachments;
	render_pass_create_info.subpassCount = 1;
	render_pass_create_info.pSubpasses   = &subpass;
	render_pass_create_info.dependencyCount = 1;
	render_pass_create_info.pDependencies   = &dependency;

	VULKAN_CALL(vkCreateRenderPass(device, &render_pass_create_info, nullptr, &render_pass));

	VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info = { };
	depth_stencil_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_create_info.depthTestEnable  = VK_TRUE;
	depth_stencil_create_info.depthWriteEnable = VK_TRUE;
	depth_stencil_create_info.depthCompareOp = VK_COMPARE_OP_LESS;
	depth_stencil_create_info.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_create_info.minDepthBounds = 0.0f;
	depth_stencil_create_info.maxDepthBounds = 1.0f;
	depth_stencil_create_info.stencilTestEnable = VK_FALSE;

	VkGraphicsPipelineCreateInfo pipeline_create_info = { };
	pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_create_info.stageCount = 2;
	pipeline_create_info.pStages    = shader_stages;

	pipeline_create_info.pVertexInputState   = &vertex_input_create_info;
	pipeline_create_info.pInputAssemblyState = &input_asm_create_info;
	pipeline_create_info.pViewportState      = &viewport_state_create_info;
	pipeline_create_info.pRasterizationState = &rasterizer_create_info;
	pipeline_create_info.pMultisampleState   = &multisample_create_info;
	pipeline_create_info.pDepthStencilState  = &depth_stencil_create_info;
	pipeline_create_info.pColorBlendState    = &blend_state_create_info;
	pipeline_create_info.pDynamicState       = nullptr;

	pipeline_create_info.layout = pipeline_layout;

	pipeline_create_info.renderPass = render_pass;
	pipeline_create_info.subpass    = 0;

	pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_create_info.basePipelineIndex  = -1;

	VULKAN_CALL(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline));
	
	vkDestroyShaderModule(device, shader_vert, nullptr);
	vkDestroyShaderModule(device, shader_frag, nullptr);
}

static void init_framebuffers() {
	framebuffers.resize(image_views.size());

	for (int i = 0; i < image_views.size(); i++) {
		VkImageView attachments[] = { image_views[i], depth_image_view };

		VkFramebufferCreateInfo framebuffer_create_info = { };
		framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_create_info.renderPass = render_pass;
		framebuffer_create_info.attachmentCount = Util::array_element_count(attachments);
		framebuffer_create_info.pAttachments    = attachments;
		framebuffer_create_info.width  = extent.width;
		framebuffer_create_info.height = extent.height;
		framebuffer_create_info.layers = 1;

		VULKAN_CALL(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &framebuffers[i]));
	}
}

static void init_command_pool() {
	VkCommandPoolCreateInfo command_pool_create_info = { };
	command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	command_pool_create_info.queueFamilyIndex = queue_family_graphics.value();
	command_pool_create_info.flags = 0;

	VULKAN_CALL(vkCreateCommandPool(device, &command_pool_create_info, nullptr, &command_pool));
}

static void init_depth_buffer() {
	create_image(
		extent.width,
		extent.height,
		DEPTH_FORMAT,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		depth_image,
		depth_image_memory
	);

	depth_image_view = create_image_view(depth_image, DEPTH_FORMAT, VK_IMAGE_ASPECT_DEPTH_BIT);
}

static void init_vertex_buffer() {
	u64 buffer_size = Util::vector_size_in_bytes(vertices);

	VkBuffer       staging_buffer;
	VkDeviceMemory staging_buffer_memory;
	create_buffer(
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		staging_buffer,
		staging_buffer_memory
	);

	create_buffer(
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		vertex_buffer,
		vertex_buffer_memory
	);

	buffer_memory_copy(staging_buffer_memory, vertices.data(), buffer_size);

	buffer_copy(vertex_buffer, staging_buffer, buffer_size);

	vkDestroyBuffer(device, staging_buffer, nullptr);
	vkFreeMemory(device, staging_buffer_memory, nullptr);
}

static void init_index_buffer() {
	VkDeviceSize buffer_size = Util::vector_size_in_bytes(indices);

	VkBuffer       staging_buffer;
	VkDeviceMemory staging_buffer_memory;
	create_buffer(
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		staging_buffer,
		staging_buffer_memory
	);
	
	create_buffer(
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		index_buffer,
		index_buffer_memory
	);

	buffer_memory_copy(staging_buffer_memory, indices.data(), buffer_size);

	buffer_copy(index_buffer, staging_buffer, buffer_size);

	vkDestroyBuffer(device, staging_buffer, nullptr);
	vkFreeMemory(device, staging_buffer_memory, nullptr);
}

static void init_texture() {
	int texture_width;
	int texture_height;
	int texture_channels;

	u8 * pixels = stbi_load("Data\\bricks.png", &texture_width, &texture_height, &texture_channels, STBI_rgb_alpha);
	if (!pixels) {
		printf("ERROR: Unable to load Texture!\n");
		abort();
	}

	VkDeviceSize texture_size = texture_width * texture_height * 4;

	VkBuffer       staging_buffer;
	VkDeviceMemory staging_buffer_memory;

	create_buffer(texture_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		staging_buffer,
		staging_buffer_memory
	);

	buffer_memory_copy(staging_buffer_memory, pixels, texture_size);

	stbi_image_free(pixels);

	create_image(
		texture_width,
		texture_height,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		texture_image,
		texture_image_memory
	);

	transition_image_layout(texture_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	buffer_copy_to_image(staging_buffer, texture_image, texture_width, texture_height);

	transition_image_layout(texture_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(device, staging_buffer, nullptr);
	vkFreeMemory(device, staging_buffer_memory, nullptr);

	texture_image_view = create_image_view(texture_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
	
	VkSamplerCreateInfo sampler_create_info = { };
	sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_create_info.magFilter = VK_FILTER_LINEAR;
	sampler_create_info.minFilter = VK_FILTER_LINEAR;
	sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_create_info.anisotropyEnable = VK_TRUE;
	sampler_create_info.maxAnisotropy    = 16.0f;
	sampler_create_info.unnormalizedCoordinates = VK_FALSE;
	sampler_create_info.compareEnable = VK_FALSE;
	sampler_create_info.compareOp     = VK_COMPARE_OP_ALWAYS;
	sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_create_info.mipLodBias = 0.0f;
	sampler_create_info.minLod     = 0.0f;
	sampler_create_info.maxLod     = 0.0f;

	VULKAN_CALL(vkCreateSampler(device, &sampler_create_info, nullptr, &texture_sampler));
}

static void init_uniform_buffers() {
	VkDeviceSize buffer_size = sizeof(UniformBufferObject);

	uniform_buffers       .resize(image_views.size());
	uniform_buffers_memory.resize(image_views.size());

	for (int i = 0; i < image_views.size(); i++) {
		create_buffer(buffer_size,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			uniform_buffers[i],
			uniform_buffers_memory[i]
		);
	}
}

static void init_descriptor_pool() {
	VkDescriptorPoolSize descriptor_pool_sizes[] = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         image_views.size() },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_views.size() },
	};

	VkDescriptorPoolCreateInfo pool_create_info = { };
	pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_create_info.poolSizeCount = Util::array_element_count(descriptor_pool_sizes);
	pool_create_info.pPoolSizes    = descriptor_pool_sizes;
	pool_create_info.maxSets = image_views.size();

	VULKAN_CALL(vkCreateDescriptorPool(device, &pool_create_info, nullptr, &descriptor_pool));
}

static void init_descriptor_sets() {
	std::vector<VkDescriptorSetLayout> layouts(image_views.size(), descriptor_set_layout);

	VkDescriptorSetAllocateInfo alloc_info = { };
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = image_views.size();
	alloc_info.pSetLayouts = layouts.data();

	descriptor_sets.resize(image_views.size());
	VULKAN_CALL(vkAllocateDescriptorSets(device, &alloc_info, descriptor_sets.data()));

	for (int i = 0; i < image_views.size(); i++) {
		VkDescriptorBufferInfo buffer_info = { };
		buffer_info.buffer = uniform_buffers[i];
		buffer_info.offset = 0;
		buffer_info.range = sizeof(UniformBufferObject);

		VkDescriptorImageInfo image_info = { };
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		image_info.imageView = texture_image_view;
		image_info.sampler   = texture_sampler;

		VkWriteDescriptorSet write_descriptor_sets[2] = { };

		write_descriptor_sets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_descriptor_sets[0].dstSet = descriptor_sets[i];
		write_descriptor_sets[0].dstBinding = 0;
		write_descriptor_sets[0].dstArrayElement = 0;
		write_descriptor_sets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		write_descriptor_sets[0].descriptorCount = 1;
		write_descriptor_sets[0].pBufferInfo = &buffer_info;

		write_descriptor_sets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_descriptor_sets[1].dstSet = descriptor_sets[i];
		write_descriptor_sets[1].dstBinding = 1;
		write_descriptor_sets[1].dstArrayElement = 0;
		write_descriptor_sets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write_descriptor_sets[1].descriptorCount = 1;
		write_descriptor_sets[1].pImageInfo = &image_info;

		vkUpdateDescriptorSets(device, Util::array_element_count(write_descriptor_sets), write_descriptor_sets, 0, nullptr);
	}
}

static void init_command_buffers() {
	command_buffers.resize(framebuffers.size());

	VkCommandBufferAllocateInfo command_buffer_alloc_info = { };
	command_buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	command_buffer_alloc_info.commandPool = command_pool;
	command_buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	command_buffer_alloc_info.commandBufferCount = command_buffers.size();

	VULKAN_CALL(vkAllocateCommandBuffers(device, &command_buffer_alloc_info, command_buffers.data()));

	for (size_t i = 0; i < command_buffers.size(); i++) {
		VkCommandBufferBeginInfo command_buffer_begin_info = { };
		command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		command_buffer_begin_info.flags = 0;
		command_buffer_begin_info.pInheritanceInfo = nullptr;

		VULKAN_CALL(vkBeginCommandBuffer(command_buffers[i], &command_buffer_begin_info));
		
		VkClearValue clear_values[2] = { };
		clear_values[0].color        = { 0.0f, 0.0f, 0.0f, 1.0f };
		clear_values[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderpass_begin_info = { };
		renderpass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderpass_begin_info.renderPass = render_pass;
		renderpass_begin_info.framebuffer = framebuffers[i];
		renderpass_begin_info.renderArea.offset = { 0, 0 };
		renderpass_begin_info.renderArea.extent = extent;
		renderpass_begin_info.clearValueCount = Util::array_element_count(clear_values);
		renderpass_begin_info.pClearValues    = clear_values;
		
		vkCmdBeginRenderPass(command_buffers[i], &renderpass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		VkBuffer vertex_buffers[] = { vertex_buffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(command_buffers[i], 0, 1, vertex_buffers, offsets);

		vkCmdBindIndexBuffer(command_buffers[i], index_buffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdBindDescriptorSets(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_sets[i], 0, nullptr);

		vkCmdDrawIndexed(command_buffers[i], indices.size(), 1, 0, 0, 0);

		vkCmdEndRenderPass(command_buffers[i]);

		VULKAN_CALL(vkEndCommandBuffer(command_buffers[i]));
	}
}

static void init_sync_primitives() {
	VkSemaphoreCreateInfo semaphore_create_info = { };
	semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fence_create_info = { };
	fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		VULKAN_CALL(vkCreateSemaphore(device, &semaphore_create_info, nullptr, &semaphores_image_available[i]));
		VULKAN_CALL(vkCreateSemaphore(device, &semaphore_create_info, nullptr, &semaphores_render_done    [i]));

		VULKAN_CALL(vkCreateFence(device, &fence_create_info, nullptr, &inflight_fences[i]));
	}

	images_in_flight.resize(image_views.size(), VK_NULL_HANDLE);
}

static void cleanup_swapchain() {
	for (VkFramebuffer const & framebuffer : framebuffers) {
		vkDestroyFramebuffer(device, framebuffer, nullptr);
	}

	vkDestroyImage(device, depth_image, nullptr);
	vkDestroyImageView(device, depth_image_view, nullptr);
	vkFreeMemory(device, depth_image_memory, nullptr);
	
	vkDestroyDescriptorPool(device, descriptor_pool, nullptr);

	vkFreeCommandBuffers(device, command_pool, command_buffers.size(), command_buffers.data());

	vkDestroyRenderPass(device, render_pass, nullptr);

	vkDestroyPipeline      (device, pipeline,        nullptr);
	vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
	
	for (size_t i = 0; i < image_views.size(); i++) {
		vkDestroyImageView(device, image_views[i], nullptr);

		vkDestroyBuffer(device, uniform_buffers[i], nullptr);
		vkFreeMemory(device, uniform_buffers_memory[i], nullptr);
	}

	vkDestroySwapchainKHR(device, swapchain, nullptr);
}

static void cleanup() {
	cleanup_swapchain();

	vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);

	vkDestroySampler(device, texture_sampler, nullptr);
	vkDestroyImageView(device, texture_image_view, nullptr);

	vkDestroyImage(device, texture_image, nullptr);
	vkFreeMemory(device, texture_image_memory, nullptr);

	vkDestroyBuffer(device, vertex_buffer, nullptr);
	vkFreeMemory(device, vertex_buffer_memory, nullptr);

	vkDestroyBuffer(device, index_buffer, nullptr);
	vkFreeMemory(device, index_buffer_memory, nullptr);

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(device, semaphores_image_available[i], nullptr);
		vkDestroySemaphore(device, semaphores_render_done    [i], nullptr);

		vkDestroyFence(device, inflight_fences[i], nullptr);
	}

	vkDestroyCommandPool(device, command_pool, nullptr);

	if (validation_layers_enabled) {
		VULKAN_PROC(vkDestroyDebugUtilsMessengerEXT)(instance, debug_messenger, nullptr);
	}

	vkDestroySurfaceKHR(instance, surface, nullptr);
	
	vkDestroyDevice(device, nullptr);
	vkDestroyInstance(instance, nullptr);
	
	glfwDestroyWindow(window);
	glfwTerminate();
}

static void recreate_swapchain() {
	int width, height;

	while (true) {
		glfwGetFramebufferSize(window, &width, &height);

		if (screen_width != 0 && screen_height != 0) break;

		glfwWaitEvents();
	}

	screen_width  = width;
	screen_height = height;

	VULKAN_CALL(vkDeviceWaitIdle(device));

	cleanup_swapchain();

	init_swapchain();
	init_pipeline();
	init_depth_buffer();
	init_framebuffers();
	init_uniform_buffers();
	init_descriptor_pool();
	init_descriptor_sets();
	init_command_buffers();
}

int main() {
	init_glfw();
	init_instance();
	init_physical_device();
	init_surface();
	init_queue_families();
	init_device();
	init_queues();
	init_swapchain();
	init_descriptor_set_layout();
	init_pipeline();
	init_command_pool();
	init_depth_buffer();
	init_framebuffers();
	init_vertex_buffer();
	init_index_buffer();
	init_texture();
	init_uniform_buffers();
	init_descriptor_pool();
	init_descriptor_sets();
	init_command_buffers();
	init_sync_primitives();

	camera.position.z = 2.0f;

	double time_curr = 0.0f;
	double time_prev = 0.0f;
	double time_delta;

	int current_frame = 0;

	// Main loop
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		time_curr = glfwGetTime();
		time_delta = time_curr - time_prev;
		time_prev = time_curr;

		camera.update(time_delta);

		VkSemaphore const & semaphore_image_available = semaphores_image_available[current_frame];
		VkSemaphore const & semaphore_render_done     = semaphores_render_done    [current_frame];

		VkFence const & fence = inflight_fences[current_frame];

		VULKAN_CALL(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));
		
		u32 image_index;
		vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, semaphore_image_available, VK_NULL_HANDLE, &image_index);
		
		if (images_in_flight[image_index] != VK_NULL_HANDLE) {
			vkWaitForFences(device, 1, &images_in_flight[image_index], VK_TRUE, UINT64_MAX);
		}
		images_in_flight[image_index] = inflight_fences[current_frame];

		VULKAN_CALL(vkResetFences(device, 1, &fence));

		VkSemaphore          wait_semaphores[] = { semaphore_image_available };
		VkPipelineStageFlags wait_stages    [] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		
		VkSemaphore signal_semaphores[] = { semaphore_render_done };

		{
			static auto time_start = std::chrono::high_resolution_clock::now();

			auto time_current = std::chrono::high_resolution_clock::now();
			float time = std::chrono::duration<float, std::chrono::seconds::period>(time_current - time_start).count();

			UniformBufferObject ubo = { };
			ubo.wvp = camera.get_view_projection() * Matrix4::create_rotation(Quaternion::axis_angle(Vector3(0.0f, 0.0f, 1.0f), time));
			
			void * data;
			vkMapMemory(device, uniform_buffers_memory[image_index], 0, sizeof(ubo), 0, &data);
			memcpy(data, &ubo, sizeof(ubo));
			vkUnmapMemory(device, uniform_buffers_memory[image_index]);
		}

		VkSubmitInfo submit_info = { };
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores    = wait_semaphores;
		submit_info.pWaitDstStageMask  = wait_stages;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers    = &command_buffers[image_index];
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores    = signal_semaphores;

		VULKAN_CALL(vkQueueSubmit(queue_graphics, 1, &submit_info, fence));
		
		VkSwapchainKHR swapchains[] = { swapchain };

		VkPresentInfoKHR present_info =  { };
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores    = signal_semaphores;
		present_info.swapchainCount = 1;
		present_info.pSwapchains    = swapchains;
		present_info.pImageIndices = &image_index;
		present_info.pResults = nullptr;

		VkResult result = vkQueuePresentKHR(queue_present, &present_info);

		if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR || framebuffer_needs_resize) {
			recreate_swapchain();
			camera.on_resize(screen_width, screen_height);

			framebuffer_needs_resize = false;
		} else {
			VULKAN_CALL(result);
		}

		current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

		Input::finish_frame();
	}

	VULKAN_CALL(vkDeviceWaitIdle(device));
	
	cleanup();

	return 0;
}
