#include "GBuffer.h"

#include <cassert>

#include "VulkanCheck.h"
#include "VulkanContext.h"
#include "VulkanMemory.h"

#include "Util.h"

struct GBufferPushConstants {
	alignas(16) Matrix4 world;
	alignas(16) Matrix4 wvp;
};

struct GBufferUBO {
	int texture_index;
};

void GBuffer::FrameBuffer::init(int swapchain_image_count, int width, int height, VkFormat format, VkImageUsageFlagBits usage) {
	VkImageAspectFlags image_aspect_mask = 0;
	VkImageLayout      image_layout;

	this->format = format;

	if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
		image_aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_layout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}

	if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
		image_aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		image_layout      = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}

	assert(image_aspect_mask != 0);
	
	auto device = VulkanContext::get_device();

	attachments.resize(swapchain_image_count);

	for (auto & attachment : attachments) {
		VkImageCreateInfo image_create_info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		image_create_info.imageType = VK_IMAGE_TYPE_2D;
		image_create_info.format = format;
		image_create_info.extent = { u32(width), u32(height), 1 };
		image_create_info.mipLevels = 1;
		image_create_info.arrayLayers = 1;
		image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
		image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_create_info.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT;

		VK_CHECK(vkCreateImage(device, &image_create_info, nullptr, &attachment.image));

		VkMemoryRequirements memory_requirements; vkGetImageMemoryRequirements(device, attachment.image, &memory_requirements);

		VkMemoryAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		alloc_info.allocationSize = memory_requirements.size;
		alloc_info.memoryTypeIndex = VulkanMemory::find_memory_type(memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	
		VK_CHECK(vkAllocateMemory(device, &alloc_info, nullptr, &attachment.memory));

		VK_CHECK(vkBindImageMemory(device, attachment.image, attachment.memory, 0));

		VkImageViewCreateInfo image_view_create_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		image_view_create_info.format = format;
		image_view_create_info.subresourceRange = { };
		image_view_create_info.subresourceRange.aspectMask = image_aspect_mask;
		image_view_create_info.subresourceRange.baseMipLevel = 0;
		image_view_create_info.subresourceRange.levelCount   = 1;
		image_view_create_info.subresourceRange.baseArrayLayer = 0;
		image_view_create_info.subresourceRange.layerCount     = 1;
		image_view_create_info.image = attachment.image;

		VK_CHECK(vkCreateImageView(device, &image_view_create_info, nullptr, &attachment.image_view));
	}
}

void GBuffer::FrameBuffer::free() {
	auto device = VulkanContext::get_device();

	for (auto & attachment : attachments) {
		vkDestroyImage    (device, attachment.image,      nullptr);
		vkDestroyImageView(device, attachment.image_view, nullptr);
		vkFreeMemory      (device, attachment.memory,     nullptr);
	}
}

void GBuffer::init(int swapchain_image_count, int width, int height, std::vector<Renderable> const & renderables, std::vector<Texture *> const & textures) {
	auto device = VulkanContext::get_device();

	buffer_width  = 2048;
	buffer_height = 2048;

	// Create Descriptor Set Layout
	VkDescriptorSetLayoutBinding layout_binding_sampler = { };
	layout_binding_sampler.binding = 1;
	layout_binding_sampler.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	layout_binding_sampler.descriptorCount = 2;
	layout_binding_sampler.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	layout_binding_sampler.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding layout_binding_ubo = { };
	layout_binding_ubo.binding = 2;
	layout_binding_ubo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	layout_binding_ubo.descriptorCount = 1;
	layout_binding_ubo.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	layout_binding_ubo.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding layout_bindings[] = {
		layout_binding_sampler,
		layout_binding_ubo
	};

	VkDescriptorSetLayoutCreateInfo layout_create_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	layout_create_info.bindingCount = Util::array_element_count(layout_bindings);
	layout_create_info.pBindings    = layout_bindings;

	VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_create_info, nullptr, &descriptor_set_layout));
	
	// Create Descriptor Pool
	VkDescriptorPoolSize descriptor_pool_sizes[] = {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, swapchain_image_count },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, swapchain_image_count },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, swapchain_image_count }
	};

	VkDescriptorPoolCreateInfo pool_create_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pool_create_info.poolSizeCount = Util::array_element_count(descriptor_pool_sizes);
	pool_create_info.pPoolSizes    = descriptor_pool_sizes;
	pool_create_info.maxSets = swapchain_image_count;

	VK_CHECK(vkCreateDescriptorPool(device, &pool_create_info, nullptr, &descriptor_pool));

	// Create Uniform Buffers
	uniform_buffers.resize(swapchain_image_count);

	auto aligned_size = Math::round_up(sizeof(GBufferUBO), VulkanContext::get_min_uniform_buffer_alignment());
	
	for (int i = 0; i < swapchain_image_count; i++) {
		uniform_buffers[i] = VulkanMemory::buffer_create(renderables.size() * aligned_size,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		);
	}

	// Allocate and update Descriptor Sets
	std::vector<VkDescriptorSetLayout> layouts(swapchain_image_count, descriptor_set_layout);

	VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = layouts.size();
	alloc_info.pSetLayouts        = layouts.data();

	descriptor_sets.resize(swapchain_image_count); VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, descriptor_sets.data()));

	for (int i = 0; i < swapchain_image_count; i++) {
		std::vector<VkDescriptorImageInfo> image_infos(textures.size());

		for (int j = 0; j < textures.size(); j++) {
			image_infos[j].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			image_infos[j].imageView = textures[j]->image_view;
			image_infos[j].sampler   = textures[j]->sampler;
		}
		
		VkWriteDescriptorSet write_descriptor_sets[2] = { };

		write_descriptor_sets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_descriptor_sets[0].dstSet = descriptor_sets[i];
		write_descriptor_sets[0].dstBinding = 1;
		write_descriptor_sets[0].dstArrayElement = 0;
		write_descriptor_sets[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write_descriptor_sets[0].descriptorCount = 2;
		write_descriptor_sets[0].pImageInfo = image_infos.data();

		VkDescriptorBufferInfo buffer_info = { };
		buffer_info.buffer = uniform_buffers[i].buffer;
		buffer_info.offset = 0;
		buffer_info.range = sizeof(GBufferUBO);

		write_descriptor_sets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_descriptor_sets[1].dstSet = descriptor_sets[i];
		write_descriptor_sets[1].dstBinding = 2;
		write_descriptor_sets[1].dstArrayElement = 0;
		write_descriptor_sets[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		write_descriptor_sets[1].descriptorCount = 1;
		write_descriptor_sets[1].pBufferInfo = &buffer_info;

		vkUpdateDescriptorSets(device, Util::array_element_count(write_descriptor_sets), write_descriptor_sets, 0, nullptr);
	}

	// Create Pipeline Layout
	auto shader_vert = VulkanContext::shader_load("Shaders/geometry.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	auto shader_frag = VulkanContext::shader_load("Shaders/geometry.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	VkPipelineShaderStageCreateInfo shader_stages[] = { shader_vert.stage_create_info, shader_frag.stage_create_info };

	auto binding_descriptions   = Mesh::Vertex::get_binding_description();
	auto attribute_descriptions = Mesh::Vertex::get_attribute_description();

	VkPipelineVertexInputStateCreateInfo vertex_input_create_info = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vertex_input_create_info.vertexBindingDescriptionCount = 1;
	vertex_input_create_info.pVertexBindingDescriptions    = &binding_descriptions;
	vertex_input_create_info.vertexAttributeDescriptionCount = attribute_descriptions.size();
	vertex_input_create_info.pVertexAttributeDescriptions    = attribute_descriptions.data();

	VkPipelineInputAssemblyStateCreateInfo input_asm_create_info = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	input_asm_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_asm_create_info.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = { 0.0f, 0.0f, float(width), float(height), 0.0f, 1.0f };

	VkRect2D scissor = { 0, 0, width, height };

	VkPipelineViewportStateCreateInfo viewport_state_create_info = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	viewport_state_create_info.viewportCount = 1;
	viewport_state_create_info.pViewports    = &viewport;
	viewport_state_create_info.scissorCount = 1;
	viewport_state_create_info.pScissors    = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer_create_info = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rasterizer_create_info.depthClampEnable = VK_FALSE;
	rasterizer_create_info.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer_create_info.lineWidth   = 1.0f;
	rasterizer_create_info.depthBiasEnable         = VK_FALSE;
	rasterizer_create_info.depthBiasConstantFactor = 0.0f;
	rasterizer_create_info.depthBiasClamp          = 0.0f;
	rasterizer_create_info.depthBiasSlopeFactor    = 0.0f;
	rasterizer_create_info.cullMode  = VK_CULL_MODE_BACK_BIT;
	rasterizer_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	VkPipelineMultisampleStateCreateInfo multisample_create_info = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	multisample_create_info.sampleShadingEnable = VK_FALSE;
	multisample_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample_create_info.minSampleShading = 1.0f;
	multisample_create_info.pSampleMask      = nullptr;
	multisample_create_info.alphaToCoverageEnable = VK_FALSE;
	multisample_create_info.alphaToOneEnable      = VK_FALSE;

	VkPipelineColorBlendAttachmentState blend[3] = { };
	blend[0].blendEnable = VK_FALSE;
	blend[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blend[0].colorBlendOp = VK_BLEND_OP_ADD;
	blend[0].alphaBlendOp = VK_BLEND_OP_ADD;
	blend[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blend[0].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	blend[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blend[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

	blend[1].blendEnable = VK_FALSE;
	blend[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blend[1].colorBlendOp = VK_BLEND_OP_ADD;
	blend[1].alphaBlendOp = VK_BLEND_OP_ADD;
	blend[1].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blend[1].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	blend[1].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blend[1].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

	blend[2].blendEnable = VK_FALSE;
	blend[2].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blend[2].colorBlendOp = VK_BLEND_OP_ADD;
	blend[2].alphaBlendOp = VK_BLEND_OP_ADD;
	blend[2].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blend[2].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	blend[2].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blend[2].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

	VkPipelineColorBlendStateCreateInfo blend_state_create_info = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	blend_state_create_info.logicOpEnable = VK_FALSE;
	blend_state_create_info.logicOp       = VK_LOGIC_OP_COPY;
	blend_state_create_info.attachmentCount = Util::array_element_count(blend);
	blend_state_create_info.pAttachments    = blend;
	blend_state_create_info.blendConstants[0] = 0.0f;
	blend_state_create_info.blendConstants[1] = 0.0f;
	blend_state_create_info.blendConstants[2] = 0.0f;
	blend_state_create_info.blendConstants[3] = 0.0f;

	VkPushConstantRange push_constants;
	push_constants.offset = 0;
	push_constants.size = sizeof(GBufferPushConstants);
	push_constants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo pipeline_layout_create_info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipeline_layout_create_info.setLayoutCount = 1;
	pipeline_layout_create_info.pSetLayouts    = &descriptor_set_layout;
	pipeline_layout_create_info.pushConstantRangeCount = 1;
	pipeline_layout_create_info.pPushConstantRanges    = &push_constants;

	VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &pipeline_layout));

	// Initialize FrameBuffers and their attachments
	frame_buffer_albedo  .init(swapchain_image_count, width, height, VK_FORMAT_R8G8B8A8_UNORM,      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
	frame_buffer_position.init(swapchain_image_count, width, height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
	frame_buffer_normal  .init(swapchain_image_count, width, height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

	auto depth_format = VulkanContext::get_supported_depth_format();
	if (!depth_format.has_value()) {
		puts("ERROR: Unable to create GBuffer because no supported depth format was found!");
		abort();
	}

	frame_buffer_depth.init(swapchain_image_count, width, height, depth_format.value(), VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

	// Create Renderpass
	VkAttachmentDescription attachments[4] = { };
	attachments[0].format = frame_buffer_albedo  .format;
	attachments[1].format = frame_buffer_position.format;
	attachments[2].format = frame_buffer_normal  .format;
	attachments[3].format = frame_buffer_depth   .format;

	for (int i = 0; i < 4; i++) {
		attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[i].loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[i].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if (i < 3) {
			attachments[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		} else {
			attachments[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
	}

	VkAttachmentReference colour_ref[3] = {
		{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
		{ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
		{ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
	};

	VkAttachmentReference depth_ref = { 3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

	VkSubpassDescription subpass = { };
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = Util::array_element_count(colour_ref);
	subpass.pColorAttachments    = colour_ref;
	subpass.pDepthStencilAttachment = &depth_ref;

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
	render_pass_create_info.attachmentCount = Util::array_element_count(attachments);
	render_pass_create_info.pAttachments    = attachments;
	render_pass_create_info.subpassCount = 1;
	render_pass_create_info.pSubpasses   = &subpass;
	render_pass_create_info.dependencyCount = Util::array_element_count(dependencies);
	render_pass_create_info.pDependencies   = dependencies;

	VK_CHECK(vkCreateRenderPass(device, &render_pass_create_info, nullptr, &render_pass));
	
	// Create FrameBuffers
	frame_buffers.resize(swapchain_image_count);

	for (int i = 0; i < swapchain_image_count; i++) {
		VkImageView attachments[4] = {
			frame_buffer_albedo  .attachments[i].image_view,
			frame_buffer_position.attachments[i].image_view,
			frame_buffer_normal  .attachments[i].image_view,
			frame_buffer_depth   .attachments[i].image_view,
		};

		VkFramebufferCreateInfo frame_buffer_create_info = { };
		frame_buffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frame_buffer_create_info.pNext = NULL;
		frame_buffer_create_info.renderPass = render_pass;
		frame_buffer_create_info.attachmentCount = Util::array_element_count(attachments);
		frame_buffer_create_info.pAttachments    = attachments;
		frame_buffer_create_info.width  = width;
		frame_buffer_create_info.height = height;
		frame_buffer_create_info.layers = 1;

		VK_CHECK(vkCreateFramebuffer(device, &frame_buffer_create_info, nullptr, &frame_buffers[i]));
	}

	// Create Pipeline
	VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	depth_stencil_create_info.depthTestEnable  = VK_TRUE;
	depth_stencil_create_info.depthWriteEnable = VK_TRUE;
	depth_stencil_create_info.depthCompareOp = VK_COMPARE_OP_LESS;
	depth_stencil_create_info.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_create_info.minDepthBounds = 0.0f;
	depth_stencil_create_info.maxDepthBounds = 1.0f;
	depth_stencil_create_info.stencilTestEnable = VK_FALSE;

	VkGraphicsPipelineCreateInfo pipeline_create_info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
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

	VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline));

	vkDestroyShaderModule(device, shader_vert.module, nullptr);
	vkDestroyShaderModule(device, shader_frag.module, nullptr);

	// Create Command Buffers
	command_buffers.resize(frame_buffers.size());

	VkCommandBufferAllocateInfo command_buffer_alloc_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	command_buffer_alloc_info.commandPool = VulkanContext::get_command_pool();
	command_buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	command_buffer_alloc_info.commandBufferCount = command_buffers.size();

	VK_CHECK(vkAllocateCommandBuffers(device, &command_buffer_alloc_info, command_buffers.data()));
}

void GBuffer::free() {
	auto device = VulkanContext::get_device();

	int swapchain_image_count = frame_buffers.size();

	for (int i = 0; i < swapchain_image_count; i++) {
		vkDestroyFramebuffer(device, frame_buffers[i], nullptr);
		
		VulkanMemory::buffer_free(uniform_buffers[i]);
	}

	frame_buffer_albedo  .free();
	frame_buffer_position.free();
	frame_buffer_normal  .free();
	frame_buffer_depth   .free();
	
	vkDestroyDescriptorPool     (device, descriptor_pool,       nullptr);
	vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);

	vkDestroyPipeline      (device, pipeline,        nullptr);
	vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
	
	vkFreeCommandBuffers(device, VulkanContext::get_command_pool(), command_buffers.size(), command_buffers.data());

	vkDestroyRenderPass(device, render_pass, nullptr);
}

void GBuffer::record_command_buffer(int image_index, int width, int height, Camera const & camera, std::vector<Renderable> const & renderables) {
	auto & command_buffer = command_buffers[image_index];
	auto & frame_buffer   = frame_buffers  [image_index];
	auto & uniform_buffer = uniform_buffers[image_index];
    auto & descriptor_set = descriptor_sets[image_index];

	VkCommandBufferBeginInfo command_buffer_begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

	// Clear values for all attachments written in the fragment shader
	VkClearValue clear_values[4] = { };
	clear_values[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clear_values[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clear_values[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clear_values[3].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo render_pass_begin_info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	render_pass_begin_info.renderPass =  render_pass;
	render_pass_begin_info.framebuffer = frame_buffers[image_index];
	render_pass_begin_info.renderArea.extent.width  = width;
	render_pass_begin_info.renderArea.extent.height = height;
	render_pass_begin_info.clearValueCount = Util::array_element_count(clear_values);
	render_pass_begin_info.pClearValues    = clear_values;

	VK_CHECK(vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info));

	vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport = { 0.0f, 0.0f, float(width), float(height), 0.0f, 1.0f };

	VkRect2D scissor = { 0, 0, width, height };
	
	vkCmdSetViewport(command_buffer, 0, 1, &viewport);
	vkCmdSetScissor (command_buffer, 0, 1, &scissor);

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	// Upload UBO
	auto aligned_size = Math::round_up(sizeof(GBufferUBO), VulkanContext::get_min_uniform_buffer_alignment());

	std::vector<std::byte> buf(renderables.size() * aligned_size);

	for (int i = 0; i < renderables.size(); i++) {
		GBufferUBO ubo = { };
		ubo.texture_index = renderables[i].texture_index;

		std::memcpy(buf.data() + i * aligned_size, &ubo, sizeof(GBufferUBO));
	}

	VulkanMemory::buffer_copy_direct(uniform_buffer, buf.data(), buf.size());
	
	// Render Renderables
	for (int i = 0; i < renderables.size(); i++) {
		auto const & renderable = renderables[i];

		GBufferPushConstants push_constants;
		push_constants.world = renderable.transform;
		push_constants.wvp   = camera.get_view_projection() * renderable.transform;

		vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GBufferPushConstants), &push_constants);

		VkBuffer vertex_buffers[] = { renderable.mesh->vertex_buffer.buffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);

		vkCmdBindIndexBuffer(command_buffer, renderable.mesh->index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

		u32 offset = i * aligned_size;
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 1, &offset);

		vkCmdDrawIndexed(command_buffer, renderable.mesh->index_count, 1, 0, 0, 0);
	}

	vkCmdEndRenderPass(command_buffer);

	VK_CHECK(vkEndCommandBuffer(command_buffer));
}
