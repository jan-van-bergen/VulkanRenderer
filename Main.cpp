#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <optional>
#include <set>

#include <string>

#include <algorithm>

#include "Util.h"
#include "Types.h"
#include "VulkanCall.h"

#include "Vector2.h"
#include "Vector3.h"

int const screen_width  = 900;
int const screen_height = 600;

static VkExtent2D extent = { screen_width, screen_height };

static GLFWwindow * window;

static bool framebuffer_needs_resize = false;

static VkSurfaceFormatKHR const format = { VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
static VkPresentModeKHR   const present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
	
std::vector<char const *> const validation_layers_names = {
	"VK_LAYER_KHRONOS_validation"
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

static VkRenderPass render_pass;
static VkPipelineLayout pipeline_layout;
static VkPipeline       pipeline;

static std::vector<VkFramebuffer> framebuffers;

static VkCommandPool                command_pool;
static std::vector<VkCommandBuffer> command_buffers;

static VkBuffer       vertex_buffer;
static VkDeviceMemory vertex_buffer_memory;

static VkBuffer       index_buffer;
static VkDeviceMemory index_buffer_memory;

static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
static std::vector<VkSemaphore> semaphores_image_available(MAX_FRAMES_IN_FLIGHT);
static std::vector<VkSemaphore> semaphores_render_done    (MAX_FRAMES_IN_FLIGHT);
	
static std::vector<VkFence> inflight_fences(MAX_FRAMES_IN_FLIGHT);
static std::vector<VkFence> images_in_flight;

struct Vertex {
	Vector2 position;
	Vector3 colour;

	static VkVertexInputBindingDescription get_binding_description() {
		VkVertexInputBindingDescription bindingDescription = { };
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static std::vector<VkVertexInputAttributeDescription> get_attribute_description() {
		std::vector<VkVertexInputAttributeDescription> attribute_descriptions(2);

		// Position
		attribute_descriptions[0].binding = 0;
		attribute_descriptions[0].location = 0;
		attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		attribute_descriptions[0].offset = offsetof(Vertex, position);

		// Colour
		attribute_descriptions[1].binding = 0;
		attribute_descriptions[1].location = 1;
		attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attribute_descriptions[1].offset = offsetof(Vertex, colour);

		return attribute_descriptions;
	}
};

static std::vector<Vertex> const vertices = {
	{ { -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
    { {  0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f } },
    { {  0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f } },
    { { -0.5f,  0.5f }, { 1.0f, 1.0f, 1.0f } }
};

static std::vector<u16> const indices = { 0, 1, 2, 2, 3, 0 };

static void glfw_framebuffer_resize_callback(GLFWwindow * window, int width, int height) {
	framebuffer_needs_resize = true;
}

static void init_glfw() {
	glfwInit();

	// Init GLFW window
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	//glfwWindowHint(GLFW_RESIZABLE,  GLFW_FALSE);
	
	window = glfwCreateWindow(screen_width, screen_height, "Vulkan", nullptr, nullptr);
	glfwSetFramebufferSizeCallback(window, glfw_framebuffer_resize_callback);
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
	uint32_t queue_families_count = 0;
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

	if (!queue_family_graphics.has_value() && !queue_family_present.has_value()) abort();	
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

	VkDeviceCreateInfo device_create_info = { };
	device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_create_info.pQueueCreateInfos    = queue_create_infos.data();
	device_create_info.queueCreateInfoCount = queue_create_infos.size();
	device_create_info.pEnabledFeatures = &device_features;
	device_create_info.enabledExtensionCount   = device_extensions.size();
	device_create_info.ppEnabledExtensionNames = device_extensions.data();

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

static void init_swapchain(u32 width, u32 height) {
	VkSurfaceCapabilitiesKHR surface_capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities);

	extent.width  = std::max(surface_capabilities.minImageExtent.width,  std::min(surface_capabilities.maxImageExtent.width , width));
	extent.height = std::max(surface_capabilities.minImageExtent.height, std::min(surface_capabilities.maxImageExtent.height, height));

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

	if (queue_family_graphics.value() != queue_family_present .value()) {
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

	std::vector<VkImage> swap_chain_images(swapchain_image_count);
	vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, swap_chain_images.data());

	image_views.resize(swapchain_image_count);

	for (int i = 0; i < swapchain_image_count; i++) {
		VkImageViewCreateInfo image_view_create_info = { };
		image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		image_view_create_info.image = swap_chain_images[i];

		image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		image_view_create_info.format   = format.format;

		image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_view_create_info.subresourceRange.baseMipLevel = 0;
		image_view_create_info.subresourceRange.levelCount   = 1;
		image_view_create_info.subresourceRange.baseArrayLayer = 0;
		image_view_create_info.subresourceRange.layerCount     = 1;

		VULKAN_CALL(vkCreateImageView(device, &image_view_create_info, nullptr, &image_views[i]));
	}
}

static VkShaderModule shader_load(VkDevice device, std::string const & filename) {
	std::vector<char> spirv = read_file(filename);

	VkShaderModuleCreateInfo create_info = { };
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = spirv.size();
	create_info.pCode = reinterpret_cast<const uint32_t*>(spirv.data());

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
	pipeline_layout_create_info.setLayoutCount = 0;
	pipeline_layout_create_info.pSetLayouts    = nullptr;
	pipeline_layout_create_info.pushConstantRangeCount = 0;
	pipeline_layout_create_info.pPushConstantRanges    = nullptr;

	VULKAN_CALL(vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &pipeline_layout));

	VkAttachmentDescription colour_attachment = { };
	colour_attachment.format = format.format;
	colour_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colour_attachment.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colour_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colour_attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colour_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colour_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colour_attachment.finalLayout   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colour_attachment_ref = { };
	colour_attachment_ref.attachment = 0;
	colour_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = { };
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colour_attachment_ref;
	
	VkSubpassDependency dependency = { };
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo render_pass_create_info = { };
	render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_create_info.attachmentCount = 1;
	render_pass_create_info.pAttachments    = &colour_attachment;
	render_pass_create_info.subpassCount = 1;
	render_pass_create_info.pSubpasses   = &subpass;
	render_pass_create_info.dependencyCount = 1;
	render_pass_create_info.pDependencies   = &dependency;

	VULKAN_CALL(vkCreateRenderPass(device, &render_pass_create_info, nullptr, &render_pass));

	VkGraphicsPipelineCreateInfo pipeline_create_info = { };
	pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_create_info.stageCount = 2;
	pipeline_create_info.pStages    = shader_stages;

	pipeline_create_info.pVertexInputState   = &vertex_input_create_info;
	pipeline_create_info.pInputAssemblyState = &input_asm_create_info;
	pipeline_create_info.pViewportState      = &viewport_state_create_info;
	pipeline_create_info.pRasterizationState = &rasterizer_create_info;
	pipeline_create_info.pMultisampleState   = &multisample_create_info;
	pipeline_create_info.pDepthStencilState  = nullptr;
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
		VkImageView attachments[] = { image_views[i] };

		VkFramebufferCreateInfo framebuffer_create_info = { };
		framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_create_info.renderPass = render_pass;
		framebuffer_create_info.attachmentCount = 1;
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

static u32 find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) {
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

static void copy_buffer(VkBuffer buffer_dst, VkBuffer buffer_src, VkDeviceSize size) {
	VkCommandBufferAllocateInfo alloc_info = { };
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandPool = command_pool;
	alloc_info.commandBufferCount = 1;

	VkCommandBuffer copy_command_buffer;
	VULKAN_CALL(vkAllocateCommandBuffers(device, &alloc_info, &copy_command_buffer));

	VkCommandBufferBeginInfo begin_info = { };
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VULKAN_CALL(vkBeginCommandBuffer(copy_command_buffer, &begin_info));

	VkBufferCopy buffer_copy = { };
	buffer_copy.srcOffset = 0;
	buffer_copy.dstOffset = 0;
	buffer_copy.size = size;
	vkCmdCopyBuffer(copy_command_buffer, buffer_src, buffer_dst, 1, &buffer_copy);

	VULKAN_CALL(vkEndCommandBuffer(copy_command_buffer));

	VkSubmitInfo submit_info = { };
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers    = &copy_command_buffer;
	vkQueueSubmit(queue_graphics, 1, &submit_info, VK_NULL_HANDLE);

	vkQueueWaitIdle(queue_graphics);

	vkFreeCommandBuffers(device, command_pool, 1, &copy_command_buffer);
}

static void init_vertex_buffer() {
	size_t buffer_size = vertices.size() * sizeof(Vertex);

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

	// Copy into staging buffer
	void * data_destination;
	vkMapMemory(device, staging_buffer_memory, 0, buffer_size, 0, &data_destination);

	memcpy(data_destination, vertices.data(), buffer_size);

	vkUnmapMemory(device, staging_buffer_memory);

	copy_buffer(vertex_buffer, staging_buffer, buffer_size);

    vkDestroyBuffer(device, staging_buffer, nullptr);
    vkFreeMemory(device, staging_buffer_memory, nullptr);
}

static void init_index_buffer() {
	VkDeviceSize buffer_size = indices.size() * sizeof(u16);

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

    void * data;
    vkMapMemory(device, staging_buffer_memory, 0, buffer_size, 0, &data);

    memcpy(data, indices.data(), (size_t) buffer_size);
    
	vkUnmapMemory(device, staging_buffer_memory);

    copy_buffer(index_buffer, staging_buffer, buffer_size);

    vkDestroyBuffer(device, staging_buffer, nullptr);
    vkFreeMemory(device, staging_buffer_memory, nullptr);
}

static void init_command_buffers() {
	command_buffers.resize(framebuffers.size());

	VkCommandBufferAllocateInfo command_buffer_alloc_info = { };
	command_buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	command_buffer_alloc_info.commandPool = command_pool;
	command_buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	command_buffer_alloc_info.commandBufferCount = static_cast<u32>(command_buffers.size());

	VULKAN_CALL(vkAllocateCommandBuffers(device, &command_buffer_alloc_info, command_buffers.data()));

	for (size_t i = 0; i < command_buffers.size(); i++) {
		VkCommandBufferBeginInfo command_buffer_begin_info = { };
		command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		command_buffer_begin_info.flags = 0;
		command_buffer_begin_info.pInheritanceInfo = nullptr;

		VULKAN_CALL(vkBeginCommandBuffer(command_buffers[i], &command_buffer_begin_info));
		
		VkClearValue clear_black = { 0.0f, 0.0f, 0.0f, 1.0f };

		VkRenderPassBeginInfo renderpass_begin_info = { };
		renderpass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderpass_begin_info.renderPass = render_pass;
		renderpass_begin_info.framebuffer = framebuffers[i];
		renderpass_begin_info.renderArea.offset = { 0, 0 };
		renderpass_begin_info.renderArea.extent = extent;
		renderpass_begin_info.clearValueCount = 1;
		renderpass_begin_info.pClearValues    = &clear_black;
		
		vkCmdBeginRenderPass(command_buffers[i], &renderpass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		VkBuffer vertex_buffers[] = { vertex_buffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(command_buffers[i], 0, 1, vertex_buffers, offsets);

		vkCmdBindIndexBuffer(command_buffers[i], index_buffer, 0, VK_INDEX_TYPE_UINT16);

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
	
	vkFreeCommandBuffers(device, command_pool, command_buffers.size(), command_buffers.data());

	vkDestroyRenderPass(device, render_pass, nullptr);

	vkDestroyPipeline      (device, pipeline,        nullptr);
	vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
	
	for (VkImageView const & image_view : image_views) {
		vkDestroyImageView(device, image_view, nullptr);
	}

	vkDestroySwapchainKHR(device, swapchain, nullptr);
}

static void cleanup() {
	cleanup_swapchain();

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
	int width  = 0;
	int height = 0;

	while (true) {
		glfwGetFramebufferSize(window, &width, &height);

		if (width != 0 && height != 0) break;

		glfwWaitEvents();
	}

	VULKAN_CALL(vkDeviceWaitIdle(device));

	cleanup_swapchain();

	init_swapchain(width, height);
	init_pipeline();
	init_framebuffers();
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
	init_swapchain(screen_width, screen_height);
	init_pipeline();
	init_framebuffers();
	init_command_pool();
	init_vertex_buffer();
	init_index_buffer();
	init_command_buffers();
	init_sync_primitives();

	int current_frame = 0;

	// Main loop
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		
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
			framebuffer_needs_resize = false;
		} else {
			VULKAN_CALL(result);
		}

		current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	VULKAN_CALL(vkDeviceWaitIdle(device));
	
	cleanup();

	return 0;
}
