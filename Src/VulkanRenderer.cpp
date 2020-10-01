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

struct PushConstants {
	alignas(16) Matrix4 world;
	alignas(16) Matrix4 wvp;
};

struct UniformBufferObject {
	int texture_index;
};

static VkShaderModule shader_load(VkDevice device, std::string const & filename) {
	std::vector<char> spirv = Util::read_file(filename);

	VkShaderModuleCreateInfo create_info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	create_info.codeSize = spirv.size();
	create_info.pCode = reinterpret_cast<const u32 *>(spirv.data());

	VkShaderModule shader; VK_CHECK(vkCreateShaderModule(device, &create_info, nullptr, &shader));

	return shader;
}

static VkPipelineShaderStageCreateInfo shader_get_stage(VkShaderModule shader_module, VkShaderStageFlagBits stage) {
	VkPipelineShaderStageCreateInfo vertex_stage_create_info = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	vertex_stage_create_info.stage = stage;
	vertex_stage_create_info.module = shader_module;
	vertex_stage_create_info.pName = "main";

	return vertex_stage_create_info;
}

VulkanRenderer::VulkanRenderer(GLFWwindow * window, u32 width, u32 height) :
	semaphores_image_available(MAX_FRAMES_IN_FLIGHT),
	semaphores_render_done    (MAX_FRAMES_IN_FLIGHT),
	inflight_fences           (MAX_FRAMES_IN_FLIGHT),
	camera(DEG_TO_RAD(110.0f), width, height)
{
	this->width  = width;
	this->height = height;

	this->window = window;

	renderables.push_back({ Mesh::load("data/monkey.obj"), 0, Matrix4::identity() });
	renderables.push_back({ Mesh::load("data/cube.obj"),   1, Matrix4::identity() });
	
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	create_swapchain();
}

VulkanRenderer::~VulkanRenderer() {
	destroy_swapchain();

	auto device = VulkanContext::get_device();

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	vkDestroyDescriptorPool(device, descriptor_pool_gui, nullptr);

	vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);

	for (auto const & texture : textures) {
		texture->free();
	}

	for (auto const & renderable : renderables) {
		VulkanMemory::buffer_free(renderable.mesh->vertex_buffer);
		VulkanMemory::buffer_free(renderable.mesh->index_buffer);
	}
	
	Mesh::free();

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(device, semaphores_image_available[i], nullptr);
		vkDestroySemaphore(device, semaphores_render_done    [i], nullptr);

		vkDestroyFence(device, inflight_fences[i], nullptr);
	}
}

void VulkanRenderer::create_descriptor_set_layout() {
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

	VK_CHECK(vkCreateDescriptorSetLayout(VulkanContext::get_device(), &layout_create_info, nullptr, &descriptor_set_layout));
}

void VulkanRenderer::create_pipeline() {
	auto device = VulkanContext::get_device();

	auto shader_vert = shader_load(device, "Shaders/vert.spv");
	auto shader_frag = shader_load(device, "Shaders/frag.spv");

	VkPipelineShaderStageCreateInfo shader_stages[] = {
		shader_get_stage(shader_vert, VK_SHADER_STAGE_VERTEX_BIT),
		shader_get_stage(shader_frag, VK_SHADER_STAGE_FRAGMENT_BIT),
	};

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

	VkPipelineColorBlendAttachmentState blend = { };
	blend.blendEnable = VK_FALSE;
	blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blend.colorBlendOp = VK_BLEND_OP_ADD;
	blend.alphaBlendOp = VK_BLEND_OP_ADD;
	blend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blend.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

	VkPipelineColorBlendStateCreateInfo blend_state_create_info = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	blend_state_create_info.logicOpEnable = VK_FALSE;
	blend_state_create_info.logicOp       = VK_LOGIC_OP_COPY;
	blend_state_create_info.attachmentCount = 1;
	blend_state_create_info.pAttachments    = &blend;
	blend_state_create_info.blendConstants[0] = 0.0f;
	blend_state_create_info.blendConstants[1] = 0.0f;
	blend_state_create_info.blendConstants[2] = 0.0f;
	blend_state_create_info.blendConstants[3] = 0.0f;

	VkPushConstantRange push_constants;
	push_constants.offset = 0;
	push_constants.size = sizeof(PushConstants);
	push_constants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo pipeline_layout_create_info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipeline_layout_create_info.setLayoutCount = 1;
	pipeline_layout_create_info.pSetLayouts    = &descriptor_set_layout;
	pipeline_layout_create_info.pushConstantRangeCount = 1;
	pipeline_layout_create_info.pPushConstantRanges    = &push_constants;

	VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &pipeline_layout));

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

	VK_CHECK(vkCreateRenderPass(device, &render_pass_create_info, nullptr, &render_pass));

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

	vkDestroyShaderModule(device, shader_vert, nullptr);
	vkDestroyShaderModule(device, shader_frag, nullptr);
}

void VulkanRenderer::create_framebuffers() {
	frame_buffers.resize(image_views.size());

	for (int i = 0; i < image_views.size(); i++) {
		VkImageView attachments[] = { image_views[i], depth_image_view };

		VkFramebufferCreateInfo framebuffer_create_info = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		framebuffer_create_info.renderPass = render_pass;
		framebuffer_create_info.attachmentCount = Util::array_element_count(attachments);
		framebuffer_create_info.pAttachments    = attachments;
		framebuffer_create_info.width  = width;
		framebuffer_create_info.height = height;
		framebuffer_create_info.layers = 1;

		VK_CHECK(vkCreateFramebuffer(VulkanContext::get_device(), &framebuffer_create_info, nullptr, &frame_buffers[i]));
	}
}

void VulkanRenderer::create_depth_buffer() {
	VulkanMemory::create_image(
		width, height,
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
	for (auto const & renderable : renderables) {
		auto buffer_size = Util::vector_size_in_bytes(renderable.mesh->vertices);

		renderable.mesh->vertex_buffer = VulkanMemory::buffer_create(
			buffer_size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);

		VulkanMemory::buffer_copy_staged(renderable.mesh->vertex_buffer, renderable.mesh->vertices.data(), buffer_size);
	}
}

void VulkanRenderer::create_index_buffer() {
	for (auto const & renderable : renderables) {
		auto buffer_size = Util::vector_size_in_bytes(renderable.mesh->indices);

		renderable.mesh->index_buffer = VulkanMemory::buffer_create(
			buffer_size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);

		VulkanMemory::buffer_copy_staged(renderable.mesh->index_buffer, renderable.mesh->indices.data(), buffer_size);
	}
}

void VulkanRenderer::create_textures() {
	textures.push_back(Texture::load("Data/bricks.png"));
	textures.push_back(Texture::load("Data/bricks2.png"));
}

void VulkanRenderer::create_uniform_buffers() {
	uniform_buffers.resize(image_views.size());

	auto aligned_size = Math::round_up(sizeof(UniformBufferObject), VulkanContext::get_min_uniform_buffer_alignment());
	
	for (int i = 0; i < image_views.size(); i++) {
		uniform_buffers[i] = VulkanMemory::buffer_create(renderables.size() * aligned_size,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		);
	}
}

void VulkanRenderer::create_descriptor_pool() {
	VkDescriptorPoolSize descriptor_pool_sizes[] = {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_views.size() },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_views.size() },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, image_views.size() }
	};

	VkDescriptorPoolCreateInfo pool_create_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pool_create_info.poolSizeCount = Util::array_element_count(descriptor_pool_sizes);
	pool_create_info.pPoolSizes    = descriptor_pool_sizes;
	pool_create_info.maxSets = image_views.size();

	VK_CHECK(vkCreateDescriptorPool(VulkanContext::get_device(), &pool_create_info, nullptr, &descriptor_pool));
}

void VulkanRenderer::create_descriptor_sets() {
	std::vector<VkDescriptorSetLayout> layouts(image_views.size(), descriptor_set_layout);

	VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = image_views.size();
	alloc_info.pSetLayouts = layouts.data();

	auto device = VulkanContext::get_device();

	descriptor_sets.resize(image_views.size());
	VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, descriptor_sets.data()));

	for (int i = 0; i < image_views.size(); i++) {
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
		buffer_info.range = sizeof(UniformBufferObject);

		write_descriptor_sets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_descriptor_sets[1].dstSet = descriptor_sets[i];
		write_descriptor_sets[1].dstBinding = 2;
		write_descriptor_sets[1].dstArrayElement = 0;
		write_descriptor_sets[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		write_descriptor_sets[1].descriptorCount = 1;
		write_descriptor_sets[1].pBufferInfo = &buffer_info;

		vkUpdateDescriptorSets(device, Util::array_element_count(write_descriptor_sets), write_descriptor_sets, 0, nullptr);
	}
}

void VulkanRenderer::create_imgui() {
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

	VK_CHECK(vkCreateDescriptorPool(VulkanContext::get_device(), &pool_create_info, nullptr, &descriptor_pool_gui));

	ImGui_ImplVulkan_InitInfo init_info = { };
	init_info.Instance       = VulkanContext::get_instance();
	init_info.PhysicalDevice = VulkanContext::get_physical_device();
	init_info.Device         = VulkanContext::get_device();
	init_info.QueueFamily = VulkanContext::get_queue_family_graphics();
	init_info.Queue       = VulkanContext::get_queue_graphics();
	init_info.PipelineCache = nullptr;
	init_info.DescriptorPool = descriptor_pool_gui;
	init_info.Allocator = nullptr;
	init_info.MinImageCount = 2;
	init_info.ImageCount    = image_views.size();
	init_info.CheckVkResultFn = [](VkResult result) { VK_CHECK(result); };

	ImGui_ImplVulkan_Init(&init_info, render_pass);

	VkCommandBuffer command_buffer = VulkanMemory::command_buffer_single_use_begin();
	ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
	VulkanMemory::command_buffer_single_use_end(command_buffer);
}

void VulkanRenderer::create_command_buffers() {
	auto device = VulkanContext::get_device();

	command_buffers.resize(frame_buffers.size());

	VkCommandBufferAllocateInfo command_buffer_alloc_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	command_buffer_alloc_info.commandPool = VulkanContext::get_command_pool();
	command_buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	command_buffer_alloc_info.commandBufferCount = command_buffers.size();

	VK_CHECK(vkAllocateCommandBuffers(device, &command_buffer_alloc_info, command_buffers.data()));
}

void VulkanRenderer::record_command_buffer(u32 image_index) {
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
	auto & uniform_buffer = uniform_buffers[image_index];
	auto & descriptor_set = descriptor_sets[image_index];

	VkCommandBufferBeginInfo command_buffer_begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	command_buffer_begin_info.flags = 0;
	command_buffer_begin_info.pInheritanceInfo = nullptr;

	VK_CHECK(vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info));
		
	VkClearValue clear_values[2] = { };
	clear_values[0].color        = { 0.0f, 0.0f, 0.0f, 1.0f };
	clear_values[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderpass_begin_info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	renderpass_begin_info.renderPass = render_pass;
	renderpass_begin_info.framebuffer = frame_buffer;
	renderpass_begin_info.renderArea.offset = { 0, 0 };
	renderpass_begin_info.renderArea.extent = { width, height };
	renderpass_begin_info.clearValueCount = Util::array_element_count(clear_values);
	renderpass_begin_info.pClearValues    = clear_values;
		
	vkCmdBeginRenderPass(command_buffer, &renderpass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	
	auto aligned_size = Math::round_up(sizeof(UniformBufferObject), VulkanContext::get_min_uniform_buffer_alignment());

	std::vector<std::byte> buf(renderables.size() * aligned_size);

	for (int i = 0; i < renderables.size(); i++) {
		UniformBufferObject ubo;
		ubo.texture_index = renderables[i].texture_index;

		std::memcpy(buf.data() + i * aligned_size, &ubo, sizeof(UniformBufferObject));
	}

	VulkanMemory::buffer_copy_direct(uniform_buffer, buf.data(), buf.size());
	
	for (int i = 0; i < renderables.size(); i++) {
		auto const & renderable = renderables[i];

		PushConstants push_constants;
		push_constants.world = renderable.transform;
		push_constants.wvp   = camera.get_view_projection() * renderable.transform;

		vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &push_constants);

		VkBuffer vertex_buffers[] = { renderable.mesh->vertex_buffer.buffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);

		vkCmdBindIndexBuffer(command_buffer, renderable.mesh->index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

		u32 offset = i * aligned_size;
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 1, &offset);

		vkCmdDrawIndexed(command_buffer, renderable.mesh->indices.size(), 1, 0, 0, 0);
	}

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
		VK_CHECK(vkCreateSemaphore(device, &semaphore_create_info, nullptr, &semaphores_render_done    [i]));

		VK_CHECK(vkCreateFence(device, &fence_create_info, nullptr, &inflight_fences[i]));
	}

	images_in_flight.resize(image_views.size(), VK_NULL_HANDLE);
}

void VulkanRenderer::create_swapchain() {
	swapchain = VulkanContext::create_swapchain(width, height);

	auto device = VulkanContext::get_device();

	u32                  swapchain_image_count;                   vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, nullptr);
	std::vector<VkImage> swapchain_images(swapchain_image_count); vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, swapchain_images.data());

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
	create_textures();
	create_uniform_buffers();
	create_descriptor_pool();
	create_descriptor_sets();
	create_command_buffers();
	create_sync_primitives();
	create_imgui();	
}

void VulkanRenderer::update(float delta) {
	frame_delta = delta;

	camera.update(delta);

	static float time = 0.0f;
	time += delta;

	renderables[0].transform = Matrix4::create_scale(1.0f + 0.5f * std::sin(time));
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
	auto const & semaphore_render_done     = semaphores_render_done    [current_frame];

	auto const & fence = inflight_fences[current_frame];

	VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));

	u32 image_index; VK_CHECK(vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, semaphore_image_available, VK_NULL_HANDLE, &image_index));

	if (images_in_flight[image_index] != VK_NULL_HANDLE) {
		vkWaitForFences(device, 1, &images_in_flight[image_index], VK_TRUE, UINT64_MAX);
	}
	images_in_flight[image_index] = inflight_fences[current_frame];

	VK_CHECK(vkResetFences(device, 1, &fence));
		
	record_command_buffer(image_index);

	VkSemaphore          wait_semaphores[] = { semaphore_image_available };
	VkPipelineStageFlags wait_stages    [] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		
	VkSemaphore signal_semaphores[] = { semaphore_render_done };

	VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores    = wait_semaphores;
	submit_info.pWaitDstStageMask  = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers    = &command_buffers[image_index];
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores    = signal_semaphores;

	VK_CHECK(vkQueueSubmit(queue_graphics, 1, &submit_info, fence));
		
	VkSwapchainKHR swapchains[] = { swapchain };

	VkPresentInfoKHR present_info =  { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores    = signal_semaphores;
	present_info.swapchainCount = 1;
	present_info.pSwapchains    = swapchains;
	present_info.pImageIndices = &image_index;
	present_info.pResults = nullptr;

	auto result = vkQueuePresentKHR(queue_present, &present_info);

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

		destroy_swapchain();
		create_swapchain();

		printf("Recreated SwapChain!\n");

		camera.on_resize(width, height);

		framebuffer_needs_resize = false;
	} else {
		VK_CHECK(result);
	}

	current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::destroy_swapchain() {
	auto device       = VulkanContext::get_device();
	auto command_pool = VulkanContext::get_command_pool();

	for (auto const & framebuffer : frame_buffers) {
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
	}

	vkDestroySwapchainKHR(device, swapchain, nullptr);
}
