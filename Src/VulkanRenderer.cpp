#include "VulkanRenderer.h"

#include <string>
#include <chrono>

#include <Imgui/imgui.h>
#include <Imgui/imgui_impl_glfw.h>
#include <Imgui/imgui_impl_vulkan.h>

#include "VulkanCheck.h"
#include "VulkanContext.h"

#include "Vector2.h"
#include "Vector3.h"
#include "Matrix4.h"

#include "Util.h"

struct PointLightPushConstants {
	alignas(16) Matrix4 wvp;
};

struct DirectionalLightUBO {
	alignas(16) Vector3 light_colour;
	alignas(16) Vector3 light_direction;

	alignas(16) Vector3 camera_position; 
};

struct PointLightUBO {
	alignas(16) Vector3 light_colour;
	alignas(16) Vector3 light_position;
	alignas(4)  float   light_radius;

	alignas(16) Vector3 camera_position;
};

VulkanRenderer::VulkanRenderer(GLFWwindow * window, u32 width, u32 height) :
	semaphores_image_available(MAX_FRAMES_IN_FLIGHT),
	semaphores_gbuffer_done   (MAX_FRAMES_IN_FLIGHT),
	semaphores_render_done    (MAX_FRAMES_IN_FLIGHT),
	inflight_fences           (MAX_FRAMES_IN_FLIGHT),
	camera(DEG_TO_RAD(110.0f), width, height)
{
	this->width  = width;
	this->height = height;

	this->window = window;

	renderables.push_back({ Mesh::load("Data/Monkey.obj"),  0, Matrix4::identity() });
	renderables.push_back({ Mesh::load("Data/Cube.obj"),    1, Matrix4::identity() });
	renderables.push_back({ Mesh::load("Data/Terrain.obj"), 0, Matrix4::create_translation(Vector3(0.0f, -7.5f, 0.0f)) });
	
	textures.push_back(Texture::load("Data/bricks.png"));
	textures.push_back(Texture::load("Data/bricks2.png"));

	directional_lights.push_back({ Vector3(1.0f), Vector3::normalize(Vector3(1.0f, -1.0f, 0.0f)) });

	point_lights.push_back({ Vector3(1.0f, 0.0f, 0.0f), Vector3(-6.0f, 0.0f, 0.0f),  4.0f });
	point_lights.push_back({ Vector3(0.0f, 0.0f, 1.0f), Vector3( 6.0f, 0.0f, 0.0f), 16.0f });

	PointLight::init_sphere();

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	swapchain_create();
}

VulkanRenderer::~VulkanRenderer() {
	auto device = VulkanContext::get_device();

	swapchain_destroy();

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	gbuffer.free();

	vkDestroyDescriptorPool(device, descriptor_pool_gui, nullptr);

	PointLight::free_sphere();

	Texture::free();
	Mesh::free();

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(device, semaphores_image_available[i], nullptr);
		vkDestroySemaphore(device, semaphores_gbuffer_done   [i], nullptr);
		vkDestroySemaphore(device, semaphores_render_done    [i], nullptr);

		vkDestroyFence(device, inflight_fences[i], nullptr);
	}
}

void VulkanRenderer::LightPass::free() {
	auto device = VulkanContext::get_device();

	vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
	vkDestroyPipeline      (device, pipeline,        nullptr);
	
	for (int i = 0; i < uniform_buffers.size(); i++) {
		VulkanMemory::buffer_free(uniform_buffers[i]);
	}
}

void VulkanRenderer::create_light_render_pass() {
	auto device = VulkanContext::get_device();
	
	// Create Descriptor Pool
	VkDescriptorPoolSize descriptor_pool_sizes[] = {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_views.size() },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_views.size() },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_views.size() },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, image_views.size() }
	};

	VkDescriptorPoolCreateInfo pool_create_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pool_create_info.poolSizeCount = Util::array_element_count(descriptor_pool_sizes);
	pool_create_info.pPoolSizes    = descriptor_pool_sizes;
	pool_create_info.maxSets = 2 * image_views.size();

	VK_CHECK(vkCreateDescriptorPool(device, &pool_create_info, nullptr, &descriptor_pool));
	
	// Create Descriptor Set Layout
	VkDescriptorSetLayoutBinding layout_bindings[4] = { };

	// Albedo Sampler
	layout_bindings[0].binding = 0;
	layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	layout_bindings[0].descriptorCount = 1;
	layout_bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	layout_bindings[0].pImmutableSamplers = nullptr;

	// Position Sampler
	layout_bindings[1].binding = 1;
	layout_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	layout_bindings[1].descriptorCount = 1;
	layout_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	layout_bindings[1].pImmutableSamplers = nullptr;

	// Normal Sampler
	layout_bindings[2].binding = 2;
	layout_bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	layout_bindings[2].descriptorCount = 1;
	layout_bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	layout_bindings[2].pImmutableSamplers = nullptr;
	
	// Uniform Buffer
	layout_bindings[3].binding = 3;
	layout_bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	layout_bindings[3].descriptorCount = 1;
	layout_bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	layout_bindings[3].pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layout_create_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	layout_create_info.bindingCount = Util::array_element_count(layout_bindings);
	layout_create_info.pBindings    = layout_bindings;

	VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_create_info, nullptr, &light_descriptor_set_layout));

	// Create Render Pass
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

	VkRenderPassCreateInfo render_pass_create_info = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	render_pass_create_info.attachmentCount = Util::array_element_count(attachments);
	render_pass_create_info.pAttachments    = attachments;
	render_pass_create_info.subpassCount = 1;
	render_pass_create_info.pSubpasses   = &subpass;
	render_pass_create_info.dependencyCount = 1;
	render_pass_create_info.pDependencies   = &dependency;

	VK_CHECK(vkCreateRenderPass(device, &render_pass_create_info, nullptr, &light_render_pass));
	
	// Create Sampler to sample from the GBuffer's color attachments
	VkSamplerCreateInfo sampler_create_info = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	sampler_create_info.magFilter = VK_FILTER_NEAREST;
	sampler_create_info.minFilter = VK_FILTER_NEAREST;
	sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_create_info.mipLodBias = 0.0f;
	sampler_create_info.maxAnisotropy = 1.0f;
	sampler_create_info.minLod = 0.0f;
	sampler_create_info.maxLod = 1.0f;
	sampler_create_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

	VK_CHECK(vkCreateSampler(device, &sampler_create_info, nullptr, &gbuffer_sampler));
}

VulkanRenderer::LightPass VulkanRenderer::create_light_pass(
	std::vector<VkVertexInputBindingDescription>   const & vertex_bindings,
	std::vector<VkVertexInputAttributeDescription> const & vertex_attributes,
	std::string const & filename_shader_vertex,
	std::string const & filename_shader_fragment,
	size_t push_constants_size,
	size_t ubo_size
) {
	auto device = VulkanContext::get_device();

	LightPass light_pass;

	// Create Pipeline Layout
	VkPipelineVertexInputStateCreateInfo vertex_input_create_info = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vertex_input_create_info.vertexBindingDescriptionCount = vertex_bindings.size();
	vertex_input_create_info.pVertexBindingDescriptions    = vertex_bindings.data();
	vertex_input_create_info.vertexAttributeDescriptionCount = vertex_attributes.size();
	vertex_input_create_info.pVertexAttributeDescriptions    = vertex_attributes.data();

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
	rasterizer_create_info.cullMode  = VK_CULL_MODE_FRONT_BIT;
	rasterizer_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	VkPipelineMultisampleStateCreateInfo multisample_create_info = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	multisample_create_info.sampleShadingEnable = VK_FALSE;
	multisample_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample_create_info.minSampleShading = 1.0f;
	multisample_create_info.pSampleMask      = nullptr;
	multisample_create_info.alphaToCoverageEnable = VK_FALSE;
	multisample_create_info.alphaToOneEnable      = VK_FALSE;

	VkPipelineColorBlendAttachmentState blend = { };
	blend.blendEnable = VK_TRUE;
	blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blend.colorBlendOp = VK_BLEND_OP_ADD;
	blend.alphaBlendOp = VK_BLEND_OP_ADD;
	blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;

	VkPipelineColorBlendStateCreateInfo blend_state_create_info = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	blend_state_create_info.logicOpEnable = VK_FALSE;
	blend_state_create_info.logicOp       = VK_LOGIC_OP_COPY;
	blend_state_create_info.attachmentCount = 1;
	blend_state_create_info.pAttachments    = &blend;

	VkPushConstantRange push_constants = { };
	push_constants.offset = 0;
	push_constants.size = push_constants_size;
	push_constants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo pipeline_layout_create_info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipeline_layout_create_info.setLayoutCount = 1;
	pipeline_layout_create_info.pSetLayouts    = &light_descriptor_set_layout;
	if (push_constants_size > 0) {
		pipeline_layout_create_info.pushConstantRangeCount = 1;
		pipeline_layout_create_info.pPushConstantRanges    = &push_constants;
	}

	VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &light_pass.pipeline_layout));

	// Create Pipeline
	auto shader_vert = VulkanContext::shader_load(filename_shader_vertex,   VK_SHADER_STAGE_VERTEX_BIT);
	auto shader_frag = VulkanContext::shader_load(filename_shader_fragment, VK_SHADER_STAGE_FRAGMENT_BIT);

	VkPipelineShaderStageCreateInfo shader_stages[] = { shader_vert.stage_create_info, shader_frag.stage_create_info };

	VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	depth_stencil_create_info.depthTestEnable  = VK_FALSE;
	depth_stencil_create_info.depthWriteEnable = VK_FALSE;
	depth_stencil_create_info.depthCompareOp = VK_COMPARE_OP_LESS;
	depth_stencil_create_info.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_create_info.minDepthBounds = 0.0f;
	depth_stencil_create_info.maxDepthBounds = 1.0f;
	depth_stencil_create_info.stencilTestEnable = VK_FALSE;

	VkGraphicsPipelineCreateInfo pipeline_create_info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipeline_create_info.stageCount = Util::array_element_count(shader_stages);
	pipeline_create_info.pStages    = shader_stages;
	pipeline_create_info.pVertexInputState   = &vertex_input_create_info;
	pipeline_create_info.pInputAssemblyState = &input_asm_create_info;
	pipeline_create_info.pViewportState      = &viewport_state_create_info;
	pipeline_create_info.pRasterizationState = &rasterizer_create_info;
	pipeline_create_info.pMultisampleState   = &multisample_create_info;
	pipeline_create_info.pDepthStencilState  = &depth_stencil_create_info;
	pipeline_create_info.pColorBlendState    = &blend_state_create_info;
	pipeline_create_info.pDynamicState       = nullptr;
	pipeline_create_info.layout = light_pass.pipeline_layout;
	pipeline_create_info.renderPass = light_render_pass;
	pipeline_create_info.subpass    = 0;
	pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_create_info.basePipelineIndex  = -1;

	VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &light_pass.pipeline));

	vkDestroyShaderModule(device, shader_vert.module, nullptr);
	vkDestroyShaderModule(device, shader_frag.module, nullptr);
		
	// Create Uniform Buffers
	light_pass.uniform_buffers.resize(image_views.size());

	auto aligned_size = Math::round_up(sizeof(PointLightUBO), VulkanContext::get_min_uniform_buffer_alignment());
	
	for (int i = 0; i < image_views.size(); i++) {
		light_pass.uniform_buffers[i] = VulkanMemory::buffer_create(point_lights.size() * aligned_size,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		);
	}

	// Allocate and update Descriptor Sets
	std::vector<VkDescriptorSetLayout> layouts(image_views.size(), light_descriptor_set_layout);

	VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = layouts.size();
	alloc_info.pSetLayouts        = layouts.data();

	light_pass.descriptor_sets.resize(image_views.size());
	VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, light_pass.descriptor_sets.data()));

	for (int i = 0; i < light_pass.descriptor_sets.size(); i++) {
		auto & descriptor_set = light_pass.descriptor_sets[i];

		VkWriteDescriptorSet write_descriptor_sets[4] = { };

		// Write Descriptor for Albedo target
		VkDescriptorImageInfo descriptor_image_albedo = { };
		descriptor_image_albedo.sampler     = gbuffer_sampler;
		descriptor_image_albedo.imageView   = gbuffer.frame_buffer_albedo.attachments[i].image_view;
		descriptor_image_albedo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		write_descriptor_sets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_descriptor_sets[0].dstSet = descriptor_set;
		write_descriptor_sets[0].dstBinding = 0;
		write_descriptor_sets[0].dstArrayElement = 0;
		write_descriptor_sets[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write_descriptor_sets[0].descriptorCount = 1;
		write_descriptor_sets[0].pImageInfo = &descriptor_image_albedo;
		
		// Write Descriptor for Position target
		VkDescriptorImageInfo descriptor_image_position = { };
		descriptor_image_position.sampler     = gbuffer_sampler;
		descriptor_image_position.imageView   = gbuffer.frame_buffer_position.attachments[i].image_view;
		descriptor_image_position.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		write_descriptor_sets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_descriptor_sets[1].dstSet = descriptor_set;
		write_descriptor_sets[1].dstBinding = 1;
		write_descriptor_sets[1].dstArrayElement = 0;
		write_descriptor_sets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write_descriptor_sets[1].descriptorCount = 1;
		write_descriptor_sets[1].pImageInfo = &descriptor_image_position;

		// Write Descriptor for Normal target
		VkDescriptorImageInfo descriptor_image_normal = { };
		descriptor_image_normal.sampler     = gbuffer_sampler;
		descriptor_image_normal.imageView   = gbuffer.frame_buffer_normal.attachments[i].image_view;
		descriptor_image_normal.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		write_descriptor_sets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_descriptor_sets[2].dstSet = descriptor_set;
		write_descriptor_sets[2].dstBinding = 2;
		write_descriptor_sets[2].dstArrayElement = 0;
		write_descriptor_sets[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write_descriptor_sets[2].descriptorCount = 1;
		write_descriptor_sets[2].pImageInfo = &descriptor_image_normal;

		VkDescriptorBufferInfo descriptor_ubo = { };
		descriptor_ubo.buffer = light_pass.uniform_buffers[i].buffer;
		descriptor_ubo.offset = 0;
		descriptor_ubo.range = ubo_size;

		write_descriptor_sets[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_descriptor_sets[3].dstSet = descriptor_set;
		write_descriptor_sets[3].dstBinding = 3;
		write_descriptor_sets[3].dstArrayElement = 0;
		write_descriptor_sets[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		if (ubo_size > 0) {
			write_descriptor_sets[3].descriptorCount = 1;
			write_descriptor_sets[3].pBufferInfo     = &descriptor_ubo;
		}

		vkUpdateDescriptorSets(device, Util::array_element_count(write_descriptor_sets), write_descriptor_sets, 0, nullptr);
	}

	return light_pass;
}

void VulkanRenderer::create_frame_buffers() {
	// Create Depth Buffer
	VulkanMemory::create_image(
		width, height, 1,
		VulkanContext::DEPTH_FORMAT,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		depth_image, depth_image_memory
	);

	depth_image_view = VulkanMemory::create_image_view(depth_image, 1, VulkanContext::DEPTH_FORMAT, VK_IMAGE_ASPECT_DEPTH_BIT);

	// Create Frame Buffers
	frame_buffers.resize(image_views.size());

	for (int i = 0; i < image_views.size(); i++) {
		VkImageView attachments[] = { image_views[i], depth_image_view };

		VkFramebufferCreateInfo framebuffer_create_info = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		framebuffer_create_info.renderPass = light_render_pass;
		framebuffer_create_info.attachmentCount = Util::array_element_count(attachments);
		framebuffer_create_info.pAttachments    = attachments;
		framebuffer_create_info.width  = width;
		framebuffer_create_info.height = height;
		framebuffer_create_info.layers = 1;

		VK_CHECK(vkCreateFramebuffer(VulkanContext::get_device(), &framebuffer_create_info, nullptr, &frame_buffers[i]));
	}
}

void VulkanRenderer::create_imgui() {
	auto device = VulkanContext::get_device();

	ImGui_ImplGlfw_InitForVulkan(window, false);

	VkDescriptorPoolSize pool_sizes[] = {
		{ VK_DESCRIPTOR_TYPE_SAMPLER,                1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       1000 }
	};

	VkDescriptorPoolCreateInfo pool_create_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pool_create_info.poolSizeCount = Util::array_element_count(pool_sizes);
	pool_create_info.pPoolSizes    = pool_sizes;
	pool_create_info.maxSets = image_views.size();

	VK_CHECK(vkCreateDescriptorPool(device, &pool_create_info, nullptr, &descriptor_pool_gui));

	ImGui_ImplVulkan_InitInfo init_info = { };
	init_info.Instance       = VulkanContext::get_instance();
	init_info.PhysicalDevice = VulkanContext::get_physical_device();
	init_info.Device         = device;
	init_info.QueueFamily = VulkanContext::get_queue_family_graphics();
	init_info.Queue       = VulkanContext::get_queue_graphics();
	init_info.PipelineCache = nullptr;
	init_info.DescriptorPool = descriptor_pool_gui;
	init_info.Allocator = nullptr;
	init_info.MinImageCount = 2;
	init_info.ImageCount    = image_views.size();
	init_info.CheckVkResultFn = [](VkResult result) { VK_CHECK(result); };

	ImGui_ImplVulkan_Init(&init_info, light_render_pass);

	VkCommandBuffer command_buffer = VulkanMemory::command_buffer_single_use_begin();
	ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

	VulkanMemory::command_buffer_single_use_end(command_buffer);
}

void VulkanRenderer::create_command_buffers() {
	auto device = VulkanContext::get_device();

	command_buffers.resize(image_views.size());

	VkCommandBufferAllocateInfo command_buffer_alloc_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	command_buffer_alloc_info.commandPool = VulkanContext::get_command_pool();
	command_buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	command_buffer_alloc_info.commandBufferCount = command_buffers.size();

	VK_CHECK(vkAllocateCommandBuffers(device, &command_buffer_alloc_info, command_buffers.data()));
}

void VulkanRenderer::record_command_buffer(u32 image_index) {
	// Capture GUI draw data
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Vulkan Renderer");
	ImGui::Text("Delta: %.2f ms", 1000.0f * frame_delta);
	ImGui::Text("FPS:   %d", fps);
	ImGui::End();

	ImGui::Render();

	auto & command_buffer = command_buffers[image_index];
	auto & frame_buffer   = frame_buffers  [image_index];

	// Begin Command Buffer
	VkCommandBufferBeginInfo command_buffer_begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	command_buffer_begin_info.flags = 0;
	command_buffer_begin_info.pInheritanceInfo = nullptr;

	VK_CHECK(vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info));
	
	VkClearValue clear_values[2] = { };
	clear_values[0].color        = { 0.0f, 0.0f, 0.0f, 1.0f };
	clear_values[1].depthStencil = { 1.0f, 0 };
	
	// Begin Light Render Pass
	VkRenderPassBeginInfo renderpass_begin_info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	renderpass_begin_info.renderPass = light_render_pass;
	renderpass_begin_info.framebuffer = frame_buffer;
	renderpass_begin_info.renderArea = { 0, 0, width, height };
	renderpass_begin_info.clearValueCount = Util::array_element_count(clear_values);
	renderpass_begin_info.pClearValues    = clear_values;
		
	vkCmdBeginRenderPass(command_buffer, &renderpass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

	// Render Directional Lights
	if (directional_lights.size() > 0) {
		auto & uniform_buffer = light_pass_directional.uniform_buffers[image_index];
		auto & descriptor_set = light_pass_directional.descriptor_sets[image_index];

		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, light_pass_directional.pipeline);
		
		// Upload Uniform Buffer
		auto aligned_size = Math::round_up(sizeof(DirectionalLightUBO), VulkanContext::get_min_uniform_buffer_alignment());
		
		std::vector<std::byte> buf(directional_lights.size() * aligned_size);

		for (int i = 0; i < directional_lights.size(); i++) {
			auto const & directional_light = directional_lights[i];

			DirectionalLightUBO ubo = { };
			ubo.light_colour    = directional_light.colour;
			ubo.light_direction = directional_light.direction;
			ubo.camera_position = camera.position;

			std::memcpy(buf.data() + i * aligned_size, &ubo, sizeof(DirectionalLightUBO));
		}
		
		VulkanMemory::buffer_copy_direct(uniform_buffer, buf.data(), buf.size());
	
		// For each Directional Light render a full sqreen quad
		for (int i = 0; i < directional_lights.size(); i++) {
			auto const & directional_light = directional_lights[i];

			u32 offset = i * aligned_size;
			vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, light_pass_directional.pipeline_layout, 0, 1, &descriptor_set, 1, &offset);

			vkCmdDraw(command_buffer, 3, 1, 0, 0);
		}
	}

	// Render Point Lights
	if (point_lights.size() > 0) {
		auto & uniform_buffer = light_pass_point.uniform_buffers[image_index];
		auto & descriptor_set = light_pass_point.descriptor_sets[image_index];

		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, light_pass_point.pipeline);

		// Upload Uniform Buffer
		auto aligned_size = Math::round_up(sizeof(PointLightUBO), VulkanContext::get_min_uniform_buffer_alignment());

		std::vector<std::byte> buf(point_lights.size() * aligned_size);

		for (int i = 0; i < point_lights.size(); i++) {
			auto const & point_light = point_lights[i];

			PointLightUBO ubo = { };
			ubo.light_colour   = point_light.colour;
			ubo.light_position = point_light.position;
			ubo.light_radius   = point_light.radius;
			ubo.camera_position = camera.position;

			std::memcpy(buf.data() + i * aligned_size, &ubo, sizeof(PointLightUBO));
		}

		VulkanMemory::buffer_copy_direct(uniform_buffer, buf.data(), buf.size());
	
		// Bind Sphere to render PointLights
		VkBuffer vertex_buffers[] = { PointLight::sphere.vertex_buffer.buffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);

		vkCmdBindIndexBuffer(command_buffer, PointLight::sphere.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

		// For each PointLight render a sphere with the appropriate radius and position
		for (int i = 0; i < point_lights.size(); i++) {
			auto const & point_light = point_lights[i];

			PointLightPushConstants push_constants = { };
			push_constants.wvp = camera.get_view_projection() * Matrix4::create_translation(point_light.position) * Matrix4::create_scale(point_light.radius);

			vkCmdPushConstants(command_buffer, light_pass_point.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PointLightPushConstants), &push_constants);

			u32 offset = i * aligned_size;
			vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, light_pass_point.pipeline_layout, 0, 1, &descriptor_set, 1, &offset);

			vkCmdDrawIndexed(command_buffer, PointLight::sphere.index_count, 1, 0, 0, 0);
		}
	}

	// Render GUI
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);
	
	vkCmdEndRenderPass(command_buffer);

	VK_CHECK(vkEndCommandBuffer(command_buffer));
}

void VulkanRenderer::create_sync_primitives() {
	auto device = VulkanContext::get_device();

	VkSemaphoreCreateInfo semaphore_create_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

	VkFenceCreateInfo fence_create_info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		VK_CHECK(vkCreateSemaphore(device, &semaphore_create_info, nullptr, &semaphores_image_available[i]));
		VK_CHECK(vkCreateSemaphore(device, &semaphore_create_info, nullptr, &semaphores_gbuffer_done   [i]));
		VK_CHECK(vkCreateSemaphore(device, &semaphore_create_info, nullptr, &semaphores_render_done    [i]));

		VK_CHECK(vkCreateFence(device, &fence_create_info, nullptr, &inflight_fences[i]));
	}

	images_in_flight.resize(image_views.size(), VK_NULL_HANDLE);
}

void VulkanRenderer::swapchain_create() {
	swapchain = VulkanContext::create_swapchain(width, height);

	auto device = VulkanContext::get_device();

	u32                  swapchain_image_count;                   vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, nullptr);
	std::vector<VkImage> swapchain_images(swapchain_image_count); vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, swapchain_images.data());

	image_views.resize(swapchain_image_count);

	for (int i = 0; i < swapchain_image_count; i++) {
		image_views[i] = VulkanMemory::create_image_view(swapchain_images[i], 1, VulkanContext::FORMAT.format, VK_IMAGE_ASPECT_COLOR_BIT);
	}

	gbuffer.init(swapchain_image_count, width, height, renderables, textures);

	create_light_render_pass();

	light_pass_directional = create_light_pass(
		{ },
		{ },
		"Shaders/light_directional.vert.spv",
		"Shaders/light_directional.frag.spv",
		0,
		sizeof(DirectionalLightUBO)
	);

	light_pass_point = create_light_pass(
		{ { 0, sizeof(Vector3), VK_VERTEX_INPUT_RATE_VERTEX } },
		{ { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 } },
		"Shaders/light_point.vert.spv",
		"Shaders/light_point.frag.spv",
		sizeof(PointLightPushConstants),
		sizeof(PointLightUBO)
	);
		
	create_frame_buffers();

	create_command_buffers();
	create_sync_primitives();
	create_imgui();	
}

void VulkanRenderer::update(float delta) {
	frame_delta = delta;

	camera.update(delta);

	static float time = 0.0f;
	time += delta;

	renderables[0].transform = Matrix4::create_scale(5.0f + std::sin(time));
	renderables[1].transform = Matrix4::create_rotation(Quaternion::axis_angle(Vector3(0.0f, 1.0f, 0.0f), time)) * Matrix4::create_translation(Vector3(10.0f, 0.0f, 0.0f));

	time_since_last_second += delta;
	frames_since_last_second++;

	while (time_since_last_second >= 1.0f) {
		time_since_last_second -= 1.0f;

		fps = frames_since_last_second;
		frames_since_last_second = 0;
	}
}

void VulkanRenderer::render() {
	auto device         = VulkanContext::get_device();
	auto queue_graphics = VulkanContext::get_queue_graphics();
	auto queue_present  = VulkanContext::get_queue_present();

	auto const & semaphore_image_available = semaphores_image_available[current_frame];
	auto const & semaphore_gbuffer_done    = semaphores_gbuffer_done   [current_frame];
	auto const & semaphore_render_done     = semaphores_render_done    [current_frame];

	auto const & fence = inflight_fences[current_frame];

	VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));

	u32 image_index; VK_CHECK(vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, semaphore_image_available, VK_NULL_HANDLE, &image_index));

	if (images_in_flight[image_index] != VK_NULL_HANDLE) {
		vkWaitForFences(device, 1, &images_in_flight[image_index], VK_TRUE, UINT64_MAX);
	}
	images_in_flight[image_index] = fence;

	VK_CHECK(vkResetFences(device, 1, &fence));

	gbuffer.record_command_buffer(image_index, width, height, camera, renderables);
	record_command_buffer(image_index);

	// Render to GBuffer
	{
		VkSemaphore          wait_semaphores[] = { semaphore_image_available };
		VkPipelineStageFlags wait_stages    [] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		
		VkSemaphore signal_semaphores[] = { semaphore_gbuffer_done };

		VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submit_info.waitSemaphoreCount = Util::array_element_count(wait_semaphores);
		submit_info.pWaitSemaphores    = wait_semaphores;
		submit_info.pWaitDstStageMask  = wait_stages;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers    = &gbuffer.command_buffers[image_index];
		submit_info.signalSemaphoreCount = Util::array_element_count(signal_semaphores);
		submit_info.pSignalSemaphores    = signal_semaphores;

		VK_CHECK(vkQueueSubmit(queue_graphics, 1, &submit_info, VK_NULL_HANDLE));
	}

	// Render to Screen
	{
		VkSemaphore          wait_semaphores[] = { semaphore_gbuffer_done };
		VkPipelineStageFlags wait_stages    [] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		
		VkSemaphore signal_semaphores[] = { semaphore_render_done };

		VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submit_info.waitSemaphoreCount = Util::array_element_count(wait_semaphores);
		submit_info.pWaitSemaphores    = wait_semaphores;
		submit_info.pWaitDstStageMask  = wait_stages;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers    = &command_buffers[image_index];
		submit_info.signalSemaphoreCount = Util::array_element_count(signal_semaphores);
		submit_info.pSignalSemaphores    = signal_semaphores;

		VK_CHECK(vkQueueSubmit(queue_graphics, 1, &submit_info, fence));
	}

	VkSwapchainKHR swapchains[] = { swapchain };

	VkPresentInfoKHR present_info =  { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores    = &semaphore_render_done;
	present_info.swapchainCount = 1;
	present_info.pSwapchains    = swapchains;
	present_info.pImageIndices  = &image_index;
	present_info.pResults = nullptr;

	auto result = vkQueuePresentKHR(queue_present, &present_info);
	
	VK_CHECK(vkDeviceWaitIdle(device));

	if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR || framebuffer_needs_resize) {
		int w, h;

		while (true) {
			glfwGetFramebufferSize(window, &w, &h);

			if (w != 0 && h != 0) break;

			glfwWaitEvents();
		}

		width  = w;
		height = h;
		
		VK_CHECK(vkDeviceWaitIdle(device));

		swapchain_destroy();
		swapchain_create();

		printf("Recreated SwapChain!\n");

		camera.on_resize(width, height);

		framebuffer_needs_resize = false;
	} else {
		VK_CHECK(result);
	}

	current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::swapchain_destroy() {
	auto device       = VulkanContext::get_device();
	auto command_pool = VulkanContext::get_command_pool();

	vkDestroyImage    (device, depth_image,        nullptr);
	vkDestroyImageView(device, depth_image_view,   nullptr);
	vkFreeMemory      (device, depth_image_memory, nullptr);
	
	vkDestroySampler(device, gbuffer_sampler, nullptr);
	
	light_pass_directional.free();
	light_pass_point      .free();

	vkDestroyDescriptorPool     (device, descriptor_pool,       nullptr);
	vkDestroyDescriptorSetLayout(device, light_descriptor_set_layout, nullptr);

	vkFreeCommandBuffers(device, command_pool, command_buffers.size(), command_buffers.data());

	vkDestroyRenderPass(device, light_render_pass, nullptr);

	for (int i = 0; i < image_views.size(); i++) {
		vkDestroyFramebuffer(device, frame_buffers[i], nullptr);
		vkDestroyImageView  (device, image_views  [i], nullptr);

	}

	vkDestroySwapchainKHR(device, swapchain, nullptr);
}
