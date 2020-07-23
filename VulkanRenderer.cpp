#include "VulkanRenderer.h"

#include <string>

#include <chrono>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#include "VulkanCall.h"
#include "VulkanContext.h"
#include "VulkanMemory.h"

#include "Vector2.h"
#include "Vector3.h"
#include "Matrix4.h"

#include "Util.h"

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

VulkanRenderer::VulkanRenderer(GLFWwindow * window, u32 width, u32 height) :
	semaphores_image_available(MAX_FRAMES_IN_FLIGHT),
	semaphores_render_done(MAX_FRAMES_IN_FLIGHT),
	inflight_fences(MAX_FRAMES_IN_FLIGHT),
	camera(DEG_TO_RAD(110.0f), width, height
) {
	this->width  = width;
	this->height = height;

	this->window = window;

	create_swapchain();
}

VulkanRenderer::~VulkanRenderer() {
	destroy_swapchain();

	VkDevice device = VulkanContext::get_device();

	vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);

	vkDestroySampler  (device, texture_sampler,      nullptr);
	vkDestroyImageView(device, texture_image_view,   nullptr);
	vkDestroyImage    (device, texture_image,        nullptr);
	vkFreeMemory      (device, texture_image_memory, nullptr);

	vkDestroyBuffer(device, vertex_buffer,        nullptr);
	vkFreeMemory   (device, vertex_buffer_memory, nullptr);

	vkDestroyBuffer(device, index_buffer,        nullptr);
	vkFreeMemory   (device, index_buffer_memory, nullptr);

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(device, semaphores_image_available[i], nullptr);
		vkDestroySemaphore(device, semaphores_render_done    [i], nullptr);

		vkDestroyFence(device, inflight_fences[i], nullptr);
	}
}

void VulkanRenderer::create_descriptor_set_layout() {
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

	VULKAN_CALL(vkCreateDescriptorSetLayout(VulkanContext::get_device(), &layout_create_info, nullptr, &descriptor_set_layout));
}

void VulkanRenderer::create_pipeline() {
	VkDevice device = VulkanContext::get_device();

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
	viewport.width  = static_cast<float>(width);
	viewport.height = static_cast<float>(height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = { };
	scissor.offset = { 0, 0 };
	scissor.extent = { width, height };

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
	attachment_colour.format = VulkanContext::FORMAT.format;
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
	attachment_depth.format = VulkanContext::DEPTH_FORMAT;
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

void VulkanRenderer::create_framebuffers() {
	framebuffers.resize(image_views.size());

	for (int i = 0; i < image_views.size(); i++) {
		VkImageView attachments[] = { image_views[i], depth_image_view };

		VkFramebufferCreateInfo framebuffer_create_info = { };
		framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_create_info.renderPass = render_pass;
		framebuffer_create_info.attachmentCount = Util::array_element_count(attachments);
		framebuffer_create_info.pAttachments    = attachments;
		framebuffer_create_info.width  = width;
		framebuffer_create_info.height = height;
		framebuffer_create_info.layers = 1;

		VULKAN_CALL(vkCreateFramebuffer(VulkanContext::get_device(), &framebuffer_create_info, nullptr, &framebuffers[i]));
	}
}

void VulkanRenderer::create_depth_buffer() {
	VulkanMemory::create_image(
		width,
		height,
		VulkanContext::DEPTH_FORMAT,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		depth_image,
		depth_image_memory
	);

	depth_image_view = VulkanMemory::create_image_view(depth_image, VulkanContext::DEPTH_FORMAT, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void VulkanRenderer::create_vertex_buffer() {
	u64 buffer_size = Util::vector_size_in_bytes(vertices);

	VkBuffer       staging_buffer;
	VkDeviceMemory staging_buffer_memory;
	VulkanMemory::create_buffer(
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		staging_buffer,
		staging_buffer_memory
	);

	VulkanMemory::create_buffer(
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		vertex_buffer,
		vertex_buffer_memory
	);

	VulkanMemory::buffer_memory_copy(staging_buffer_memory, vertices.data(), buffer_size);

	VulkanMemory::buffer_copy(vertex_buffer, staging_buffer, buffer_size);

	VkDevice device = VulkanContext::get_device();

	vkDestroyBuffer(device, staging_buffer,        nullptr);
	vkFreeMemory   (device, staging_buffer_memory, nullptr);
}

void VulkanRenderer::create_index_buffer() {
	VkDeviceSize buffer_size = Util::vector_size_in_bytes(indices);

	VkBuffer       staging_buffer;
	VkDeviceMemory staging_buffer_memory;
	VulkanMemory::create_buffer(
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		staging_buffer,
		staging_buffer_memory
	);
	
	VulkanMemory::create_buffer(
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		index_buffer,
		index_buffer_memory
	);

	VulkanMemory::buffer_memory_copy(staging_buffer_memory, indices.data(), buffer_size);

	VulkanMemory::buffer_copy(index_buffer, staging_buffer, buffer_size);
	
	VkDevice device = VulkanContext::get_device();

	vkDestroyBuffer(device, staging_buffer,        nullptr);
	vkFreeMemory   (device, staging_buffer_memory, nullptr);
}

void VulkanRenderer::create_texture() {
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

	VulkanMemory::create_buffer(texture_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		staging_buffer,
		staging_buffer_memory
	);

	VulkanMemory::buffer_memory_copy(staging_buffer_memory, pixels, texture_size);

	stbi_image_free(pixels);

	VulkanMemory::create_image(
		texture_width,
		texture_height,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		texture_image,
		texture_image_memory
	);

	VulkanMemory::transition_image_layout(texture_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	VulkanMemory::buffer_copy_to_image(staging_buffer, texture_image, texture_width, texture_height);

	VulkanMemory::transition_image_layout(texture_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	VkDevice device = VulkanContext::get_device();

	vkDestroyBuffer(device, staging_buffer,        nullptr);
	vkFreeMemory   (device, staging_buffer_memory, nullptr);

	texture_image_view = VulkanMemory::create_image_view(texture_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
	
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

void VulkanRenderer::create_uniform_buffers() {
	VkDeviceSize buffer_size = sizeof(UniformBufferObject);

	uniform_buffers       .resize(image_views.size());
	uniform_buffers_memory.resize(image_views.size());

	for (int i = 0; i < image_views.size(); i++) {
		VulkanMemory::create_buffer(buffer_size,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			uniform_buffers[i],
			uniform_buffers_memory[i]
		);
	}
}

void VulkanRenderer::create_descriptor_pool() {
	VkDescriptorPoolSize descriptor_pool_sizes[] = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         image_views.size() },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_views.size() },
	};

	VkDescriptorPoolCreateInfo pool_create_info = { };
	pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_create_info.poolSizeCount = Util::array_element_count(descriptor_pool_sizes);
	pool_create_info.pPoolSizes    = descriptor_pool_sizes;
	pool_create_info.maxSets = image_views.size();

	VULKAN_CALL(vkCreateDescriptorPool(VulkanContext::get_device(), &pool_create_info, nullptr, &descriptor_pool));
}

void VulkanRenderer::create_descriptor_sets() {
	std::vector<VkDescriptorSetLayout> layouts(image_views.size(), descriptor_set_layout);

	VkDescriptorSetAllocateInfo alloc_info = { };
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = image_views.size();
	alloc_info.pSetLayouts = layouts.data();

	VkDevice device = VulkanContext::get_device();

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

void VulkanRenderer::create_command_buffers() {
	VkDevice device = VulkanContext::get_device();

	command_buffers.resize(framebuffers.size());

	VkCommandBufferAllocateInfo command_buffer_alloc_info = { };
	command_buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	command_buffer_alloc_info.commandPool = VulkanContext::get_command_pool();
	command_buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	command_buffer_alloc_info.commandBufferCount = command_buffers.size();

	VULKAN_CALL(vkAllocateCommandBuffers(device, &command_buffer_alloc_info, command_buffers.data()));

	for (int i = 0; i < command_buffers.size(); i++) {
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
		renderpass_begin_info.renderArea.extent = { width, height };
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

void VulkanRenderer::create_sync_primitives() {
	VkDevice device = VulkanContext::get_device();

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

void VulkanRenderer::create_swapchain() {
	this->swapchain = VulkanContext::create_swapchain(width, height);

	VkDevice device = VulkanContext::get_device();

	u32 swapchain_image_count;
	vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, nullptr);

	std::vector<VkImage> swapchain_images(swapchain_image_count);
	vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, swapchain_images.data());

	image_views.resize(swapchain_image_count);

	for (int i = 0; i < swapchain_image_count; i++) {
		image_views[i] = VulkanMemory::create_image_view(swapchain_images[i], VulkanContext::FORMAT.format, VK_IMAGE_ASPECT_COLOR_BIT);
	}

	create_descriptor_set_layout();
	create_pipeline();
	create_depth_buffer();
	create_framebuffers();
	create_vertex_buffer();
	create_index_buffer();
	create_texture();
	create_uniform_buffers();
	create_descriptor_pool();
	create_descriptor_sets();
	create_command_buffers();
	create_sync_primitives();
}

void VulkanRenderer::render() {
	VkDevice device        = VulkanContext::get_device();
	VkQueue queue_graphics = VulkanContext::get_queue_graphics();
	VkQueue queue_present  = VulkanContext::get_queue_present();

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
		int w, h;

		while (true) {
			glfwGetFramebufferSize(window, &w, &h);

			if (w != 0 && h != 0) break;

			glfwWaitEvents();
		}

		width  = w;
		height = h;
		
		VULKAN_CALL(vkDeviceWaitIdle(device));

		destroy_swapchain();
		create_swapchain();

		camera.on_resize(width, height);

		framebuffer_needs_resize = false;
	} else {
		VULKAN_CALL(result);
	}

	current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::destroy_swapchain() {
	VkDevice      device       = VulkanContext::get_device();
	VkCommandPool command_pool = VulkanContext::get_command_pool();

	for (VkFramebuffer const & framebuffer : framebuffers) {
		vkDestroyFramebuffer(device, framebuffer, nullptr);
	}

	vkDestroyImage    (device, depth_image,        nullptr);
	vkDestroyImageView(device, depth_image_view,   nullptr);
	vkFreeMemory      (device, depth_image_memory, nullptr);
	
	vkDestroyDescriptorPool(device, descriptor_pool, nullptr);

	vkFreeCommandBuffers(device, command_pool, command_buffers.size(), command_buffers.data());

	vkDestroyRenderPass(device, render_pass, nullptr);

	vkDestroyPipeline      (device, pipeline,        nullptr);
	vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
	
	for (int i = 0; i < image_views.size(); i++) {
		vkDestroyImageView(device, image_views[i], nullptr);

		vkDestroyBuffer(device, uniform_buffers       [i], nullptr);
		vkFreeMemory   (device, uniform_buffers_memory[i], nullptr);
	}

	vkDestroySwapchainKHR(device, swapchain, nullptr);
}
