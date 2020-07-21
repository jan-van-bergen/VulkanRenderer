#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <optional>
#include <set>

#include <algorithm>

#include "Types.h"
#include "VulkanCall.h"

#include "Shader.h"

int const screen_width  = 900;
int const screen_height = 600;

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

int main() {
	glfwInit();

	// Init GLFW window
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE,  GLFW_FALSE);
	
	GLFWwindow * window = glfwCreateWindow(screen_width, screen_height, "Vulkan", nullptr, nullptr);
	
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
	
	// Debug callback
	VkDebugUtilsMessengerEXT           debug_messenger = nullptr;
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
	
	VkInstance instance;
	VULKAN_CALL(vkCreateInstance(&instance_create_info, nullptr, &instance));
	
	if (validation_layers_enabled) {
		VULKAN_PROC(vkCreateDebugUtilsMessengerEXT)(instance, &callback_create_info, nullptr, &debug_messenger);
	}

	u32 device_count = 0;
	vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
	
	if (device_count == 0) {
		printf("ERROR: No Physical Devices available!\n");
		abort();
	}

	std::vector<VkPhysicalDevice> devices(device_count);
	vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
	
	VkPhysicalDevice physical_device = devices[0];

	
	u32 available_extensions_count = 0;
	VULKAN_CALL(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &available_extensions_count, nullptr));
	
	std::vector<VkExtensionProperties> available_extensions(available_extensions_count);
	VULKAN_CALL(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &available_extensions_count, available_extensions.data()));

	//for (char const * required_extension : device_extensions) {
	//	bool found = false;
	//	for (VkExtensionProperties const & properties : available_extensions) {
	//		if (strcmp(required_extension, properties.extensionName) == 0) {
	//			found = true;
	//			break;
	//		}
	//	}

	//	if (!found) {
	//		printf("ERROR: Extension '%s' is not available!\n", required_extension);
	//		abort();
	//	}
	//}

	uint32_t queue_families_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, nullptr);

	std::vector<VkQueueFamilyProperties> queue_families(queue_families_count);
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, queue_families.data());
	
	VkSurfaceKHR surface;
	VULKAN_CALL(glfwCreateWindowSurface(instance, window, nullptr, &surface));

	std::optional<u32> queue_family_graphics;
	std::optional<u32> queue_family_present;

	for (int i = 0; i < queue_families_count; i++) {
		VkBool32 supports_graphics = queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
		VkBool32 supports_present  = false;
		VULKAN_CALL(vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &supports_present));

		if (supports_graphics) queue_family_graphics = i;
		if (supports_present)  queue_family_present  = i;

		if (queue_family_graphics.has_value() && queue_family_present.has_value()) break;
	}

	if (!queue_family_graphics.has_value() && !queue_family_present.has_value()) abort();
	
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

	VkDeviceQueueCreateInfo queue_create_info = { };
	queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_create_info.queueFamilyIndex = queue_family_graphics.value();
	queue_create_info.queueCount = 1;
	queue_create_info.pQueuePriorities = &queue_priority;
	
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
	
	VkDevice device;
	VULKAN_CALL(vkCreateDevice(physical_device, &device_create_info, nullptr, &device));

	VkQueue queue_graphics;
	VkQueue queue_present;
	vkGetDeviceQueue(device, queue_family_graphics.value(), 0, &queue_graphics);
	vkGetDeviceQueue(device, queue_family_present .value(), 0, &queue_present);

	VkSurfaceFormatKHR format = { VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	VkPresentModeKHR   present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
	
	VkSurfaceCapabilitiesKHR surface_capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities);

	VkExtent2D extent = { screen_width, screen_height };

	extent.width  = std::max(surface_capabilities.minImageExtent.width,  std::min(surface_capabilities.maxImageExtent.width , extent.width));
	extent.height = std::max(surface_capabilities.minImageExtent.height, std::min(surface_capabilities.maxImageExtent.height, extent.height));

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

	VkSwapchainKHR swap_chain;
	VULKAN_CALL(vkCreateSwapchainKHR(device, &swapchain_create_info, nullptr, &swap_chain));

	u32 swap_chain_image_count;
	vkGetSwapchainImagesKHR(device, swap_chain, &swap_chain_image_count, nullptr);

	std::vector<VkImage> swap_chain_images(swap_chain_image_count);
	vkGetSwapchainImagesKHR(device, swap_chain, &swap_chain_image_count, swap_chain_images.data());

	std::vector<VkImageView> image_views(swap_chain_image_count);

	for (int i = 0; i < swap_chain_image_count; i++) {
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

	Shader shader_vert = Shader::load(device, "Shaders\\vert.spv");
	Shader shader_frag = Shader::load(device, "Shaders\\frag.spv");

	VkPipelineShaderStageCreateInfo shader_stages[] = {
		shader_vert.get_stage(VK_SHADER_STAGE_VERTEX_BIT),
		shader_frag.get_stage(VK_SHADER_STAGE_FRAGMENT_BIT),
	};

	VkPipelineVertexInputStateCreateInfo vertex_input_create_info = { };
	vertex_input_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_create_info.vertexBindingDescriptionCount = 0;
	vertex_input_create_info.pVertexBindingDescriptions    = nullptr;
	vertex_input_create_info.vertexAttributeDescriptionCount = 0;
	vertex_input_create_info.pVertexAttributeDescriptions    = nullptr;

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

	VkPipelineLayout pipeline_layout;
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

	VkRenderPassCreateInfo render_pass_create_info = { };
	render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_create_info.attachmentCount = 1;
	render_pass_create_info.pAttachments    = &colour_attachment;
	render_pass_create_info.subpassCount = 1;
	render_pass_create_info.pSubpasses   = &subpass;

	VkRenderPass render_pass;
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

	VkPipeline pipeline;
	VULKAN_CALL(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline));

	std::vector<VkFramebuffer> framebuffers(image_views.size());

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

	VkCommandPoolCreateInfo command_pool_create_info = { };
	command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	command_pool_create_info.queueFamilyIndex = queue_family_graphics.value();
	command_pool_create_info.flags = 0;

	VkCommandPool command_pool;
	VULKAN_CALL(vkCreateCommandPool(device, &command_pool_create_info, nullptr, &command_pool));
	
	std::vector<VkCommandBuffer> command_buffers(framebuffers.size());

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
		vkCmdDraw(command_buffers[i], 3, 1, 0, 0);
		vkCmdEndRenderPass(command_buffers[i]);

		VULKAN_CALL(vkEndCommandBuffer(command_buffers[i]));
	}

	// Main loop
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
	}
	
	// Cleanup
	vkDestroyCommandPool(device, command_pool, nullptr);

	for (VkFramebuffer const & framebuffer : framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

	vkDestroyRenderPass(device, render_pass, nullptr);

	vkDestroyPipeline      (device, pipeline,        nullptr);
	vkDestroyPipelineLayout(device, pipeline_layout, nullptr);

	shader_vert.destroy(device);
	shader_frag.destroy(device);

	for (VkImageView const & image_view : image_views) {
        vkDestroyImageView(device, image_view, nullptr);
    }

	vkDestroySwapchainKHR(device, swap_chain, nullptr);

	if (validation_layers_enabled) {
		VULKAN_PROC(vkDestroyDebugUtilsMessengerEXT)(instance, debug_messenger, nullptr);
	}

	vkDestroySurfaceKHR(instance, surface, nullptr);
	
	vkDestroyDevice(device, nullptr);
	vkDestroyInstance(instance, nullptr);

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}
