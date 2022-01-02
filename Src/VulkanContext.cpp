#include "VulkanContext.h"

#include <set>

#include "VulkanCheck.h"
#include "VulkanMemory.h"

#include "Math.h"
#include "Util.h"

static VkInstance instance;

static VkDebugUtilsMessengerEXT debug_messenger = nullptr;
static VkPhysicalDevice physical_device;

static VkDevice device;

static VkSurfaceKHR surface;

static u32 queue_family_graphics;
static u32 queue_family_compute;
static u32 queue_family_present;

static VkQueue queue_graphics;
static VkQueue queue_compute;
static VkQueue queue_present;

static VkCommandPool command_pool;

static size_t min_uniform_buffer_alignment;

#ifdef NDEBUG
static constexpr bool validation_layers_enabled = false;
#else
static constexpr bool validation_layers_enabled = true;
#endif

std::vector<char const *> const validation_layers_names = {
	"VK_LAYER_KHRONOS_validation",
	//"VK_LAYER_LUNARG_standard_validation"
	//"VK_LAYER_RENDERDOC_Capture"
};

static VkBool32 debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT       msg_severity,
	VkDebugUtilsMessageTypeFlagsEXT              msg_type,
	VkDebugUtilsMessengerCallbackDataEXT const * callback_data,
	void                                       * user_data
) {
	if (msg_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		printf("VULKAN Debug Callback: '%s'\n", callback_data->pMessage);

		__debugbreak();
	}

	return VK_FALSE;
}

std::vector<char const *> const device_extensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#define VULKAN_PROC(func_name) ( (PFN_##func_name)vkGetInstanceProcAddr(instance, #func_name) )

static void init_instance() {
	// Get GLFW required extensions
	u32  glfw_extension_count = 0;
	auto glfw_extensions      = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

	std::vector<char const *> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

	// Init validation layers
	if (validation_layers_enabled) {
		u32 layer_count;                                    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
		std::vector<VkLayerProperties> layers(layer_count); vkEnumerateInstanceLayerProperties(&layer_count, layers.data());

		for (char const * layer_name : validation_layers_names) {
			bool available = false;

			for (VkLayerProperties const & layer : layers) {
				if (strcmp(layer_name, layer.layerName) == 0) {
					available = true;
					break;
				}
			}

			if (!available) {
				printf("WARNING: Validation Layer '%s' unavailable!\n", layer_name);
			}
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

	VK_CHECK(vkCreateInstance(&instance_create_info, nullptr, &instance));

	if (validation_layers_enabled) {
		VULKAN_PROC(vkCreateDebugUtilsMessengerEXT)(instance, &callback_create_info, nullptr, &debug_messenger);
	}
}

static void init_physical_device() {
	u32                           device_count = 0;      vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
	std::vector<VkPhysicalDevice> devices(device_count); vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

	if (device_count == 0) {
		printf("ERROR: No Physical Devices available!\n");
		abort();
	}

	physical_device = devices[0];

	VkPhysicalDeviceProperties properties; vkGetPhysicalDeviceProperties(physical_device, &properties);

	min_uniform_buffer_alignment = properties.limits.minUniformBufferOffsetAlignment;

	printf("Picked Device Name: %s\n", properties.deviceName);
}

static void init_surface(GLFWwindow * window) {
	VK_CHECK(glfwCreateWindowSurface(instance, window, nullptr, &surface));
}

static void init_queue_families() {
	u32                                  queue_families_count = 0;             vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, nullptr);
	std::vector<VkQueueFamilyProperties> queue_families(queue_families_count); vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, queue_families.data());

	std::optional<u32> opt_queue_family_graphics;
	std::optional<u32> opt_queue_family_compute;
	std::optional<u32> opt_queue_family_present;

	for (int i = 0; i < queue_families_count; i++) {
		bool supports_graphics = queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
		bool supports_compute  = queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT;

		VkBool32 supports_present = false;
		VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &supports_present));

		if (supports_graphics) opt_queue_family_graphics = i;
		if (supports_compute)  opt_queue_family_compute  = i;
		if (supports_present)  opt_queue_family_present  = i;

		if (opt_queue_family_graphics.has_value() && opt_queue_family_compute.has_value() && opt_queue_family_present.has_value()) break;
	}

	if (!opt_queue_family_graphics.has_value() || !opt_queue_family_compute.has_value() || !opt_queue_family_present.has_value()) {
		printf("Failed to create queue families!\n");
		abort();
	}

	queue_family_graphics = opt_queue_family_graphics.value();
	queue_family_compute  = opt_queue_family_compute .value();
	queue_family_present  = opt_queue_family_present .value();
}

static void init_device() {
	auto queue_priority = 1.0f;

	std::set<u32> unique_queue_families = {
		queue_family_graphics,
		queue_family_present
	};
	std::vector<VkDeviceQueueCreateInfo> queue_create_infos;

	for (auto queue_family : unique_queue_families) {
		VkDeviceQueueCreateInfo queue_create_info = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
		queue_create_info.queueFamilyIndex = queue_family;
		queue_create_info.queueCount       = 1;
		queue_create_info.pQueuePriorities = &queue_priority;
		queue_create_infos.push_back(queue_create_info);
	}

	VkPhysicalDeviceFeatures device_features = { };
	device_features.samplerAnisotropy = true;
	device_features.independentBlend = true;
	device_features.depthBiasClamp = true;

	VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures dsf = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES };
	dsf.separateDepthStencilLayouts = true;

	VkDeviceCreateInfo device_create_info = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	device_create_info.queueCreateInfoCount = queue_create_infos.size();
	device_create_info.pQueueCreateInfos    = queue_create_infos.data();
	device_create_info.pEnabledFeatures = &device_features;
	device_create_info.enabledExtensionCount   = device_extensions.size();
	device_create_info.ppEnabledExtensionNames = device_extensions.data();
	device_create_info.pNext = &dsf;

	if constexpr (validation_layers_enabled) {
		device_create_info.enabledLayerCount   = validation_layers_names.size();
		device_create_info.ppEnabledLayerNames = validation_layers_names.data();
	}

	VK_CHECK(vkCreateDevice(physical_device, &device_create_info, nullptr, &device));
}

static void init_queues() {
	vkGetDeviceQueue(device, queue_family_graphics, 0, &queue_graphics);
	vkGetDeviceQueue(device, queue_family_compute,  0, &queue_compute);
	vkGetDeviceQueue(device, queue_family_present,  0, &queue_present);
}

static void init_command_pool() {
	VkCommandPoolCreateInfo command_pool_create_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	command_pool_create_info.queueFamilyIndex = queue_family_graphics;
	command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VK_CHECK(vkCreateCommandPool(device, &command_pool_create_info, nullptr, &command_pool));
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

	VkSwapchainKHR swapchain; VK_CHECK(vkCreateSwapchainKHR(device, &swapchain_create_info, nullptr, &swapchain));

	return swapchain;
}

VkFormat VulkanContext::get_supported_depth_format() {
	VkFormat depth_formats[] = {
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D16_UNORM
	};

	for (auto const & format : depth_formats) {
		VkFormatProperties properties; vkGetPhysicalDeviceFormatProperties(physical_device, format, &properties);

		// Format must support depth stencil attachment for optimal tiling
		if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
			return format;
		}
	}

	puts("ERROR: No suitable depth format was found!");
	abort();
}

VulkanContext::Shader VulkanContext::shader_load(std::string const & filename, VkShaderStageFlagBits stage) {
	std::vector<char> spirv = Util::read_file(filename);

	VkShaderModuleCreateInfo create_info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	create_info.codeSize = spirv.size();
	create_info.pCode = reinterpret_cast<const u32 *>(spirv.data());

	Shader shader;

	VK_CHECK(vkCreateShaderModule(device, &create_info, nullptr, &shader.module));

	shader.stage_create_info = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	shader.stage_create_info.stage  = stage;
	shader.stage_create_info.module = shader.module;
	shader.stage_create_info.pName = "main";

	return shader;
}

VkRenderPass VulkanContext::create_render_pass(std::vector<VkAttachmentDescription> const & attachments) {
	auto device = VulkanContext::get_device();

	std::vector<VkAttachmentReference> refs_colour;
	std::vector<VkAttachmentReference> refs_depth;

	for (int i = 0; i < attachments.size(); i++) {
		auto depth_formats = {
			VK_FORMAT_D16_UNORM,
			VK_FORMAT_X8_D24_UNORM_PACK32,
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D16_UNORM_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D32_SFLOAT_S8_UINT
		};
		bool is_depth_format = std::find(depth_formats.begin(), depth_formats.end(), attachments[i].format) != depth_formats.end();

		if (is_depth_format) {
			refs_depth.push_back({ u32(i), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL });
		} else {
			refs_colour.push_back({ u32(i), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
		}
	}

	VkSubpassDescription subpass = { };
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = refs_colour.size();
	subpass.pColorAttachments    = refs_colour.data();
	subpass.pDepthStencilAttachment = refs_depth.size() > 0 ? refs_depth.data() : nullptr;

	VkSubpassDependency dependencies[2] = { };

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo render_pass_create_info = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	render_pass_create_info.attachmentCount = attachments.size();
	render_pass_create_info.pAttachments    = attachments.data();
	render_pass_create_info.subpassCount = 1;
	render_pass_create_info.pSubpasses   = &subpass;
	render_pass_create_info.dependencyCount = Util::array_element_count(dependencies);
	render_pass_create_info.pDependencies   = dependencies;

	VkRenderPass render_pass; VK_CHECK(vkCreateRenderPass(device, &render_pass_create_info, nullptr, &render_pass));

	return render_pass;
}

VkPipelineLayout VulkanContext::create_pipeline_layout(PipelineLayoutDetails const & details) {
	VkPipelineLayoutCreateInfo pipeline_layout_create_info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipeline_layout_create_info.setLayoutCount = details.descriptor_set_layouts.size();
	pipeline_layout_create_info.pSetLayouts    = details.descriptor_set_layouts.data();;
	pipeline_layout_create_info.pushConstantRangeCount = details.push_constants.size();
	pipeline_layout_create_info.pPushConstantRanges    = details.push_constants.data();

	VkPipelineLayout pipeline_layout; VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &pipeline_layout));

	return pipeline_layout;
}

VkPipeline VulkanContext::create_pipeline(PipelineDetails const & details) {
	// Create Pipeline Layout
	VkPipelineVertexInputStateCreateInfo vertex_input_create_info = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vertex_input_create_info.vertexBindingDescriptionCount = details.vertex_bindings.size();
	vertex_input_create_info.pVertexBindingDescriptions    = details.vertex_bindings.data();
	vertex_input_create_info.vertexAttributeDescriptionCount = details.vertex_attributes.size();
	vertex_input_create_info.pVertexAttributeDescriptions    = details.vertex_attributes.data();

	VkPipelineInputAssemblyStateCreateInfo input_asm_create_info = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	input_asm_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_asm_create_info.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = { 0.0f, 0.0f, float(details.width), float(details.height), 0.0f, 1.0f };

	VkRect2D scissor = { 0, 0, details.width, details.height };

	VkPipelineViewportStateCreateInfo viewport_state_create_info = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	viewport_state_create_info.viewportCount = 1;
	viewport_state_create_info.pViewports    = &viewport;
	viewport_state_create_info.scissorCount = 1;
	viewport_state_create_info.pScissors    = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer_create_info = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rasterizer_create_info.depthClampEnable = VK_FALSE;
	rasterizer_create_info.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer_create_info.lineWidth   = 1.0f;
	rasterizer_create_info.depthBiasEnable         = details.enable_depth_bias;
	rasterizer_create_info.depthBiasConstantFactor = 1.25f;
	rasterizer_create_info.depthBiasClamp          = 0.0f;
	rasterizer_create_info.depthBiasSlopeFactor    = 2.5f;
	rasterizer_create_info.cullMode  = details.cull_mode;
	rasterizer_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	VkPipelineMultisampleStateCreateInfo multisample_create_info = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	multisample_create_info.sampleShadingEnable = VK_FALSE;
	multisample_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample_create_info.minSampleShading = 1.0f;
	multisample_create_info.pSampleMask      = nullptr;
	multisample_create_info.alphaToCoverageEnable = VK_FALSE;
	multisample_create_info.alphaToOneEnable      = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo blend_state_create_info = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	blend_state_create_info.logicOpEnable = VK_FALSE;
	blend_state_create_info.logicOp       = VK_LOGIC_OP_COPY;
	blend_state_create_info.attachmentCount = details.blends.size();
	blend_state_create_info.pAttachments    = details.blends.data();

	std::vector<Shader>                          shaders(details.shaders.size());
	std::vector<VkPipelineShaderStageCreateInfo> stages (details.shaders.size());

	for (int i = 0; i < details.shaders.size(); i++) {
		auto [filename, stage] = details.shaders[i];

		shaders[i] = VulkanContext::shader_load(filename, stage);
		stages [i] = shaders[i].stage_create_info;
	}

	VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	depth_stencil_create_info.depthTestEnable  = details.enable_depth_test;
	depth_stencil_create_info.depthWriteEnable = details.enable_depth_write;
	depth_stencil_create_info.depthCompareOp = details.depth_compare;
	depth_stencil_create_info.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_create_info.minDepthBounds = 0.0f;
	depth_stencil_create_info.maxDepthBounds = 1.0f;
	depth_stencil_create_info.stencilTestEnable = VK_FALSE;

	VkGraphicsPipelineCreateInfo pipeline_create_info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipeline_create_info.stageCount = stages.size();
	pipeline_create_info.pStages    = stages.data();
	pipeline_create_info.pVertexInputState   = &vertex_input_create_info;
	pipeline_create_info.pInputAssemblyState = &input_asm_create_info;
	pipeline_create_info.pViewportState      = &viewport_state_create_info;
	pipeline_create_info.pRasterizationState = &rasterizer_create_info;
	pipeline_create_info.pMultisampleState   = &multisample_create_info;
	pipeline_create_info.pDepthStencilState  = &depth_stencil_create_info;
	pipeline_create_info.pColorBlendState    = &blend_state_create_info;
	pipeline_create_info.pDynamicState       = nullptr;
	pipeline_create_info.layout     = details.pipeline_layout;
	pipeline_create_info.renderPass = details.render_pass;
	pipeline_create_info.subpass    = 0;
	pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_create_info.basePipelineIndex  = -1;

	VkPipeline pipeline; VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline));

	for (auto & shader : shaders) {
		vkDestroyShaderModule(device, shader.module, nullptr);
	}

	return pipeline;
}

VkFramebuffer VulkanContext::create_frame_buffer(int width, int height, VkRenderPass render_pass, std::vector<VkImageView> const & attachments) {
	VkFramebufferCreateInfo framebuffer_create_info = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	framebuffer_create_info.renderPass = render_pass;
	framebuffer_create_info.attachmentCount = attachments.size();
	framebuffer_create_info.pAttachments    = attachments.data();
	framebuffer_create_info.width  = width;
	framebuffer_create_info.height = height;
	framebuffer_create_info.layers = 1;

	VkFramebuffer frame_buffer; VK_CHECK(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &frame_buffer));

	return frame_buffer;
}

VkClearValue VulkanContext::create_clear_value_colour(float r, float g, float b, float a) {
	VkClearValue clear = { };
	clear.color = { r, g, b, a };
	return clear;
}

VkClearValue VulkanContext::create_clear_value_depth(float depth, u32 stencil) {
	VkClearValue clear = { };
	clear.depthStencil = { depth, stencil };
	return clear;
}

VkInstance VulkanContext::get_instance() { return instance; }

VkPhysicalDevice VulkanContext::get_physical_device() { return physical_device; }
VkDevice         VulkanContext::get_device()          { return device; }

VkSurfaceKHR VulkanContext::get_surface() { return surface; }

u32 VulkanContext::get_queue_family_graphics() { return queue_family_graphics; }
u32 VulkanContext::get_queue_family_present()  { return queue_family_present; }

VkQueue VulkanContext::get_queue_graphics() { return queue_graphics; }
VkQueue VulkanContext::get_queue_present()  { return queue_present;  }

VkCommandPool VulkanContext::get_command_pool() { return command_pool; }

size_t VulkanContext::get_min_uniform_buffer_alignment() { return min_uniform_buffer_alignment; }
