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

struct DirectionalLightUBO {
	struct {
		alignas(16) Vector3 colour;

		alignas(16) Vector3 direction;
	} directional_light;
	
	alignas(16) Vector3 camera_position; 
};

struct PointLightPushConstants {
	alignas(16) Matrix4 wvp;
};

struct PointLightUBO {
	struct {
		alignas(16) Vector3 colour;

		alignas(16) Vector3 position;
		alignas(4)  float   one_over_radius_squared;
	} point_light;

	alignas(16) Vector3 camera_position;
};

struct SpotLightUBO {
	struct {
		alignas(16) Vector3 colour;

		alignas(16) Vector3 position;
		alignas(4)  float   one_over_radius_squared;

		alignas(16) Vector3 direction;
		alignas(4)  float   cutoff_inner;
		alignas(4)  float   cutoff_outer;
	} spot_light;
	
	alignas(16) Vector3 camera_position;
};

VulkanRenderer::VulkanRenderer(GLFWwindow * window, u32 width, u32 height) : scene(width, height) {
	auto device = VulkanContext::get_device();

	this->width  = width;
	this->height = height;

	this->window = window;

	PointLight::init_sphere();

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	swapchain_create();

	VkSemaphoreCreateInfo semaphore_create_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

	VkFenceCreateInfo fence_create_info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		VK_CHECK(vkCreateSemaphore(device, &semaphore_create_info, nullptr, &semaphores_image_available[i]));
		VK_CHECK(vkCreateSemaphore(device, &semaphore_create_info, nullptr, &semaphores_gbuffer_done   [i]));
		VK_CHECK(vkCreateSemaphore(device, &semaphore_create_info, nullptr, &semaphores_render_done    [i]));

		VK_CHECK(vkCreateFence(device, &fence_create_info, nullptr, &fences[i]));
	}
}

VulkanRenderer::~VulkanRenderer() {
	auto device = VulkanContext::get_device();

	swapchain_destroy();

	ImGui::DestroyContext();

	PointLight::free_sphere();

	Texture::free();
	Mesh::free();

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(device, semaphores_image_available[i], nullptr);
		vkDestroySemaphore(device, semaphores_gbuffer_done   [i], nullptr);
		vkDestroySemaphore(device, semaphores_render_done    [i], nullptr);

		vkDestroyFence(device, fences[i], nullptr);
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
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, swapchain_views.size() * 4 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, swapchain_views.size() * 4 }
	};

	VkDescriptorPoolCreateInfo pool_create_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pool_create_info.poolSizeCount = Util::array_element_count(descriptor_pool_sizes);
	pool_create_info.pPoolSizes    = descriptor_pool_sizes;
	pool_create_info.maxSets = 4 * swapchain_views.size();

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

	// Create Post-Process Render Targets
	post_process.render_target_colour.init(width, height, VK_FORMAT_R16G16B16A16_SFLOAT,               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
	post_process.render_target_depth .init(width, height, VulkanContext::get_supported_depth_format(), VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

	// Create Render Pass
	VkAttachmentDescription attachments[2] = {  };

	attachments[0].format = post_process.render_target_colour.format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	attachments[1].format = post_process.render_target_depth.format;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference attachment_ref_colour = { };
	attachment_ref_colour.attachment = 0;
	attachment_ref_colour.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference attachment_ref_depth = { };
	attachment_ref_depth.attachment = 1;
	attachment_ref_depth.layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = { };
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount    = 1;
	subpass.pColorAttachments       = &attachment_ref_colour;
	subpass.pDepthStencilAttachment = &attachment_ref_depth;

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
	std::vector<VkPushConstantRange> push_constants(1);
	push_constants[0].offset = 0;
	push_constants[0].size = push_constants_size;
	push_constants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VulkanContext::PipelineLayoutDetails pipeline_layout_details;
	pipeline_layout_details.descriptor_set_layout = light_descriptor_set_layout;
	if (push_constants_size > 0) {
		pipeline_layout_details.push_constants = push_constants;
	}

	light_pass.pipeline_layout = VulkanContext::create_pipeline_layout(pipeline_layout_details);

	// Create Pipeline
	VulkanContext::PipelineDetails pipeline_details;
	pipeline_details.vertex_bindings   = vertex_bindings;
	pipeline_details.vertex_attributes = vertex_attributes;
	pipeline_details.width  = width;
	pipeline_details.height = height;
	pipeline_details.cull_mode = VK_CULL_MODE_FRONT_BIT;
	pipeline_details.blends = { VulkanContext::PipelineDetails::BLEND_ADDITIVE };
	pipeline_details.filename_shader_vertex   = filename_shader_vertex;
	pipeline_details.filename_shader_fragment = filename_shader_fragment;
	pipeline_details.enable_depth_test  = false;
	pipeline_details.enable_depth_write = false;
	pipeline_details.pipeline_layout = light_pass.pipeline_layout;
	pipeline_details.render_pass     = light_render_pass;

	light_pass.pipeline = VulkanContext::create_pipeline(pipeline_details);

	// Create Uniform Buffers
	light_pass.uniform_buffers.resize(swapchain_views.size());

	auto aligned_size = Math::round_up(sizeof(PointLightUBO), VulkanContext::get_min_uniform_buffer_alignment());
	
	for (int i = 0; i < swapchain_views.size(); i++) {
		light_pass.uniform_buffers[i] = VulkanMemory::buffer_create(scene.point_lights.size() * aligned_size,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		);
	}

	// Allocate and update Descriptor Sets
	std::vector<VkDescriptorSetLayout> layouts(swapchain_views.size(), light_descriptor_set_layout);

	VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = layouts.size();
	alloc_info.pSetLayouts        = layouts.data();

	light_pass.descriptor_sets.resize(swapchain_views.size());
	VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, light_pass.descriptor_sets.data()));

	for (int i = 0; i < light_pass.descriptor_sets.size(); i++) {
		auto & descriptor_set = light_pass.descriptor_sets[i];

		VkWriteDescriptorSet write_descriptor_sets[4] = { };

		// Write Descriptor for Albedo target
		VkDescriptorImageInfo descriptor_image_albedo = { };
		descriptor_image_albedo.sampler     = gbuffer_sampler;
		descriptor_image_albedo.imageView   = gbuffer.render_target_albedo.image_view;
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
		descriptor_image_position.imageView   = gbuffer.render_target_position.image_view;
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
		descriptor_image_normal.imageView   = gbuffer.render_target_normal.image_view;
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

void VulkanRenderer::create_post_process() {
	auto device = VulkanContext::get_device();

	// Create Descriptor Set Layout
	VkDescriptorSetLayoutBinding layout_bindings[1] = { };

	// Colour Sampler
	layout_bindings[0].binding = 0;
	layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	layout_bindings[0].descriptorCount = 1;
	layout_bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	layout_bindings[0].pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layout_create_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	layout_create_info.bindingCount = Util::array_element_count(layout_bindings);
	layout_create_info.pBindings    = layout_bindings;

	VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_create_info, nullptr, &post_process.descriptor_set_layout));

	// Create Render Pass
	VkAttachmentDescription attachments[2] = { };

	attachments[0].format = VulkanContext::FORMAT.format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	attachments[1].format = VulkanContext::get_supported_depth_format();
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colour_ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentReference depth_ref  = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

	VkSubpassDescription subpass = { };
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colour_ref;
	subpass.pDepthStencilAttachment = &depth_ref;

	VkSubpassDependency dependency = { };
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo render_pass_create_info = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	render_pass_create_info.attachmentCount = Util::array_element_count(attachments);
	render_pass_create_info.pAttachments    = attachments;
	render_pass_create_info.subpassCount = 1;
	render_pass_create_info.pSubpasses = &subpass;
	render_pass_create_info.dependencyCount = 1;
	render_pass_create_info.pDependencies = &dependency;

	VK_CHECK(vkCreateRenderPass(device, &render_pass_create_info, nullptr, &post_process.render_pass));

	// Create Frame Buffer
	post_process.frame_buffer = VulkanContext::create_frame_buffer(width, height, light_render_pass, {
		post_process.render_target_colour.image_view,
		post_process.render_target_depth .image_view
	});

	// Create Pipeline Layout
	VulkanContext::PipelineLayoutDetails pipeline_layout_details = { };
	pipeline_layout_details.descriptor_set_layout = post_process.descriptor_set_layout;

	post_process.pipeline_layout = VulkanContext::create_pipeline_layout(pipeline_layout_details);

	// Create Pipeline
	VulkanContext::PipelineDetails pipeline_details;
	pipeline_details.width  = width;
	pipeline_details.height = height;
	pipeline_details.cull_mode = VK_CULL_MODE_FRONT_BIT;
	pipeline_details.blends = { VulkanContext::PipelineDetails::BLEND_NONE };
	pipeline_details.filename_shader_vertex   = "Shaders/post_process.vert.spv";
	pipeline_details.filename_shader_fragment = "Shaders/post_process.frag.spv";
	pipeline_details.enable_depth_test  = false;
	pipeline_details.enable_depth_write = false;
	pipeline_details.pipeline_layout = post_process.pipeline_layout;
	pipeline_details.render_pass = post_process.render_pass;

	post_process.pipeline = VulkanContext::create_pipeline(pipeline_details);

	// Create Sampler
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

	VK_CHECK(vkCreateSampler(device, &sampler_create_info, nullptr, &post_process.sampler));

	// Allocate and update Descriptor Sets
	std::vector<VkDescriptorSetLayout> layouts(swapchain_views.size(), post_process.descriptor_set_layout);

	VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = layouts.size();
	alloc_info.pSetLayouts        = layouts.data();

	post_process.descriptor_sets.resize(swapchain_views.size());
	VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, post_process.descriptor_sets.data()));

	for (int i = 0; i < post_process.descriptor_sets.size(); i++) {
		auto & descriptor_set = post_process.descriptor_sets[i];

		VkWriteDescriptorSet write_descriptor_sets[1] = { };

		VkDescriptorImageInfo descriptor_image_colour = { };
		descriptor_image_colour.sampler     = post_process.sampler;
		descriptor_image_colour.imageView   = post_process.render_target_colour.image_view;
		descriptor_image_colour.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		write_descriptor_sets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_descriptor_sets[0].dstSet = descriptor_set;
		write_descriptor_sets[0].dstBinding = 0;
		write_descriptor_sets[0].dstArrayElement = 0;
		write_descriptor_sets[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write_descriptor_sets[0].descriptorCount = 1;
		write_descriptor_sets[0].pImageInfo = &descriptor_image_colour;

		vkUpdateDescriptorSets(device, Util::array_element_count(write_descriptor_sets), write_descriptor_sets, 0, nullptr);
	}
}

void VulkanRenderer::create_frame_buffers() {
	auto depth_format = VulkanContext::get_supported_depth_format();

	// Create Depth Buffer
	VulkanMemory::create_image(
		width, height, 1,
		depth_format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		depth_image, depth_image_memory
	);

	depth_image_view = VulkanMemory::create_image_view(depth_image, 1, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);

	// Create Frame Buffers
	frame_buffers.resize(swapchain_views.size());

	for (int i = 0; i < swapchain_views.size(); i++) {
		frame_buffers[i] = VulkanContext::create_frame_buffer(width, height, post_process.render_pass, { swapchain_views[i], depth_image_view });
	}
}

void VulkanRenderer::create_imgui() {
	auto device = VulkanContext::get_device();

	ImGui_ImplGlfw_InitForVulkan(window, true);

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
	pool_create_info.maxSets = swapchain_views.size();

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
	init_info.ImageCount    = swapchain_views.size();
	init_info.CheckVkResultFn = [](VkResult result) { VK_CHECK(result); };

	ImGui_ImplVulkan_Init(&init_info, post_process.render_pass);

	VkCommandBuffer command_buffer = VulkanMemory::command_buffer_single_use_begin();
	ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

	VulkanMemory::command_buffer_single_use_end(command_buffer);
}

void VulkanRenderer::create_command_buffers() {
	auto device = VulkanContext::get_device();

	command_buffers.resize(swapchain_views.size());

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
	ImGui::Text("Avg:   %.2f ms", 1000.0f * frame_avg);
	ImGui::Text("Min:   %.2f ms", 1000.0f * frame_min);
	ImGui::Text("Max:   %.2f ms", 1000.0f * frame_max);
	ImGui::Text("FPS:   %d", fps);
	ImGui::End();

	static MeshInstance * selected_mesh = nullptr;

	ImGui::Begin("Scene");
	for (auto & mesh : scene.meshes) {
		if (ImGui::Button(mesh.name.c_str())) {
			selected_mesh = &mesh;
		}
	}
	
	if (selected_mesh) {
		ImGui::Text("Selected Mesh: %s", selected_mesh->name.c_str());
		ImGui::SliderFloat3("Position", selected_mesh->transform.position.data, -10.0f, 10.0f);
		ImGui::SliderFloat4("Rotation", selected_mesh->transform.rotation.data,  -1.0f, -1.0f);
		ImGui::SliderFloat ("Scale",   &selected_mesh->transform.scale, 0.0f, 10.0f);
	}

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
	renderpass_begin_info.framebuffer = post_process.frame_buffer;
	renderpass_begin_info.renderArea = { 0, 0, width, height };
	renderpass_begin_info.clearValueCount = Util::array_element_count(clear_values);
	renderpass_begin_info.pClearValues    = clear_values;
		
	vkCmdBeginRenderPass(command_buffer, &renderpass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

	// Render Directional Lights
	if (scene.directional_lights.size() > 0) {
		auto & uniform_buffer = light_pass_directional.uniform_buffers[image_index];
		auto & descriptor_set = light_pass_directional.descriptor_sets[image_index];

		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, light_pass_directional.pipeline);
		
		// Upload Uniform Buffer
		auto aligned_size = Math::round_up(sizeof(DirectionalLightUBO), VulkanContext::get_min_uniform_buffer_alignment());
		
		std::vector<std::byte> buf(scene.directional_lights.size() * aligned_size);

		for (int i = 0; i < scene.directional_lights.size(); i++) {
			auto const & directional_light = scene.directional_lights[i];

			DirectionalLightUBO ubo = { };
			ubo.directional_light.colour    = directional_light.colour;
			ubo.directional_light.direction = directional_light.direction;
			ubo.camera_position = scene.camera.position;

			std::memcpy(buf.data() + i * aligned_size, &ubo, sizeof(DirectionalLightUBO));
		}
		
		VulkanMemory::buffer_copy_direct(uniform_buffer, buf.data(), buf.size());
	
		// For each Directional Light render a full sqreen quad
		for (int i = 0; i < scene.directional_lights.size(); i++) {
			auto const & directional_light = scene.directional_lights[i];

			u32 offset = i * aligned_size;
			vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, light_pass_directional.pipeline_layout, 0, 1, &descriptor_set, 1, &offset);

			vkCmdDraw(command_buffer, 3, 1, 0, 0);
		}
	}

	// Render Point Lights
	if (scene.point_lights.size() > 0) {
		auto & uniform_buffer = light_pass_point.uniform_buffers[image_index];
		auto & descriptor_set = light_pass_point.descriptor_sets[image_index];

		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, light_pass_point.pipeline);

		// Upload Uniform Buffer
		auto aligned_size = Math::round_up(sizeof(PointLightUBO), VulkanContext::get_min_uniform_buffer_alignment());

		std::vector<std::byte> buf(scene.point_lights.size() * aligned_size);

		// Bind Sphere to render Point Lights
		VkBuffer vertex_buffers[] = { PointLight::sphere.vertex_buffer.buffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);

		vkCmdBindIndexBuffer(command_buffer, PointLight::sphere.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		
		int num_unculled_lights = 0;

		// For each Point Light render a sphere with the appropriate radius and position
		for (int i = 0; i < scene.point_lights.size(); i++) {
			auto const & point_light = scene.point_lights[i];

			if (scene.camera.frustum.intersect_sphere(point_light.position, point_light.radius) == Camera::Frustum::IntersectionType::FULLY_OUTSIDE) continue;
			
			// Upload UBO
			PointLightUBO ubo = { };
			ubo.point_light.colour   = point_light.colour;
			ubo.point_light.position = point_light.position;
			ubo.point_light.one_over_radius_squared = 1.0f / (point_light.radius * point_light.radius);
			ubo.camera_position = scene.camera.position;

			std::memcpy(buf.data() + num_unculled_lights * aligned_size, &ubo, sizeof(PointLightUBO));

			// Draw Sphere
			PointLightPushConstants push_constants = { };
			push_constants.wvp = scene.camera.get_view_projection() * Matrix4::create_translation(point_light.position) * Matrix4::create_scale(point_light.radius);

			vkCmdPushConstants(command_buffer, light_pass_point.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PointLightPushConstants), &push_constants);

			u32 offset = num_unculled_lights * aligned_size;
			vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, light_pass_point.pipeline_layout, 0, 1, &descriptor_set, 1, &offset);

			vkCmdDrawIndexed(command_buffer, PointLight::sphere.index_count, 1, 0, 0, 0);

			num_unculled_lights++;
		}
		
		VulkanMemory::buffer_copy_direct(uniform_buffer, buf.data(), num_unculled_lights * aligned_size);
	}
	
	// Render Spot Lights
	if (scene.spot_lights.size() > 0) {
		auto & uniform_buffer = light_pass_spot.uniform_buffers[image_index];
		auto & descriptor_set = light_pass_spot.descriptor_sets[image_index];

		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, light_pass_spot.pipeline);

		// Upload Uniform Buffer
		auto aligned_size = Math::round_up(sizeof(SpotLightUBO), VulkanContext::get_min_uniform_buffer_alignment());

		std::vector<std::byte> buf(scene.spot_lights.size() * aligned_size);

		// Bind Sphere to render Spot Lights
		VkBuffer vertex_buffers[] = { PointLight::sphere.vertex_buffer.buffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);

		vkCmdBindIndexBuffer(command_buffer, PointLight::sphere.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		
		int num_unculled_lights = 0;

		// For each Spot Light render a sphere with the appropriate radius and position
		for (int i = 0; i < scene.spot_lights.size(); i++) {
			auto const & spot_light = scene.spot_lights[i];
			
			if (scene.camera.frustum.intersect_sphere(spot_light.position, spot_light.radius) == Camera::Frustum::IntersectionType::FULLY_OUTSIDE) continue;
			
			SpotLightUBO ubo = { };
			ubo.spot_light.colour    = spot_light.colour;
			ubo.spot_light.position  = spot_light.position;
			ubo.spot_light.direction = spot_light.direction;
			ubo.spot_light.one_over_radius_squared = 1.0f / (spot_light.radius * spot_light.radius);
			ubo.spot_light.cutoff_inner = std::cos(0.5f * spot_light.cutoff_inner);
			ubo.spot_light.cutoff_outer = std::cos(0.5f * spot_light.cutoff_outer);
			ubo.camera_position = scene.camera.position;

			std::memcpy(buf.data() + num_unculled_lights * aligned_size, &ubo, sizeof(SpotLightUBO));

			PointLightPushConstants push_constants = { };
			push_constants.wvp = scene.camera.get_view_projection() * Matrix4::create_translation(spot_light.position) * Matrix4::create_scale(spot_light.radius);

			vkCmdPushConstants(command_buffer, light_pass_spot.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PointLightPushConstants), &push_constants);

			u32 offset = num_unculled_lights * aligned_size;
			vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, light_pass_spot.pipeline_layout, 0, 1, &descriptor_set, 1, &offset);

			vkCmdDrawIndexed(command_buffer, PointLight::sphere.index_count, 1, 0, 0, 0);

			num_unculled_lights++;
		}
		
		VulkanMemory::buffer_copy_direct(uniform_buffer, buf.data(), num_unculled_lights * aligned_size);
	}

	vkCmdEndRenderPass(command_buffer);

	renderpass_begin_info.renderPass = post_process.render_pass;
	renderpass_begin_info.framebuffer = frame_buffer;
	renderpass_begin_info.renderArea = { 0, 0, width, height };
	renderpass_begin_info.clearValueCount = Util::array_element_count(clear_values);
	renderpass_begin_info.pClearValues    = clear_values;

	vkCmdBeginRenderPass(command_buffer, &renderpass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline      (command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, post_process.pipeline);
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, post_process.pipeline_layout, 0, 1, &post_process.descriptor_sets[image_index], 0, nullptr);

	vkCmdDraw(command_buffer, 3, 1, 0, 0);

	// Render GUI
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);
	
	vkCmdEndRenderPass(command_buffer);

	VK_CHECK(vkEndCommandBuffer(command_buffer));
}

void VulkanRenderer::swapchain_create() {
	swapchain = VulkanContext::create_swapchain(width, height);

	auto device = VulkanContext::get_device();

	u32                  swapchain_image_count;                   vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, nullptr);
	std::vector<VkImage> swapchain_images(swapchain_image_count); vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, swapchain_images.data());

	swapchain_views .resize(swapchain_image_count);
	fences_in_flight.resize(swapchain_image_count, nullptr);

	for (int i = 0; i < swapchain_image_count; i++) {
		swapchain_views[i] = VulkanMemory::create_image_view(swapchain_images[i], 1, VulkanContext::FORMAT.format, VK_IMAGE_ASPECT_COLOR_BIT);
	}

	gbuffer.init(swapchain_image_count, width, height);

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

	light_pass_spot = create_light_pass(
		{ { 0, sizeof(Vector3), VK_VERTEX_INPUT_RATE_VERTEX } },
		{ { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 } },
		"Shaders/light_spot.vert.spv",
		"Shaders/light_spot.frag.spv",
		sizeof(PointLightPushConstants),
		sizeof(SpotLightUBO)
	);

	create_post_process();

	create_frame_buffers();

	create_command_buffers();
	create_imgui();	
}

void VulkanRenderer::update(float delta) {
	scene.update(delta);

	frame_delta = delta;

	constexpr int FRAME_HISTORY_LENGTH = 100;

	if (frame_times.size() < 100) {
		frame_times.push_back(frame_delta);
	} else {
		frame_times[frame_index++ % FRAME_HISTORY_LENGTH] = frame_delta;
	}
	
	frame_avg = 0.0f;
	frame_min = INFINITY;
	frame_max = 0.0f;

	for (int i = 0; i < frame_times.size(); i++) {
		frame_avg += frame_times[i];
		frame_min = std::min(frame_min, frame_times[i]);
		frame_max = std::max(frame_max, frame_times[i]);
	}
	frame_avg /= float(frame_times.size());

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

	auto semaphore_image_available = semaphores_image_available[current_frame];
	auto semaphore_gbuffer_done    = semaphores_gbuffer_done   [current_frame];
	auto semaphore_render_done     = semaphores_render_done    [current_frame];

	auto fence = fences[current_frame];

	VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));
	
	u32 image_index; VK_CHECK(vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, semaphore_image_available, VK_NULL_HANDLE, &image_index));

	if (fences_in_flight[image_index] != nullptr) {
		VK_CHECK(vkWaitForFences(device, 1, &fences_in_flight[image_index], VK_TRUE, UINT64_MAX));
	}
	fences_in_flight[image_index] = fence;

	gbuffer.record_command_buffer(image_index, scene);
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

		VK_CHECK(vkResetFences(device, 1, &fence));
		VK_CHECK(vkQueueSubmit(queue_graphics, 1, &submit_info, fence));
	}

	VkSwapchainKHR swapchains[] = { swapchain };

	VkPresentInfoKHR present_info =  { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores    = &semaphore_render_done;
	present_info.swapchainCount = Util::array_element_count(swapchains);
	present_info.pSwapchains    = swapchains;
	present_info.pImageIndices  = &image_index;
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

		swapchain_destroy();
		swapchain_create();

		printf("Recreated SwapChain!\n");

		scene.camera.on_resize(width, height);

		framebuffer_needs_resize = false;
	} else {
		VK_CHECK(result);
	}

	current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::swapchain_destroy() {
	auto device       = VulkanContext::get_device();
	auto command_pool = VulkanContext::get_command_pool();
	
	gbuffer.free();

	vkDestroyImage    (device, depth_image,        nullptr);
	vkDestroyImageView(device, depth_image_view,   nullptr);
	vkFreeMemory      (device, depth_image_memory, nullptr);
	
	vkDestroySampler(device, gbuffer_sampler, nullptr);
	
	light_pass_directional.free();
	light_pass_point      .free();
	light_pass_spot       .free();

	vkDestroyDescriptorPool     (device, descriptor_pool,                    nullptr);
	vkDestroyDescriptorSetLayout(device, light_descriptor_set_layout,        nullptr);
	vkDestroyDescriptorSetLayout(device, post_process.descriptor_set_layout, nullptr);

	vkFreeCommandBuffers(device, command_pool, command_buffers.size(), command_buffers.data());

	vkDestroyRenderPass(device, light_render_pass,        nullptr);
	vkDestroyRenderPass(device, post_process.render_pass, nullptr);

	post_process.render_target_colour.free();
	post_process.render_target_depth .free();

	vkDestroyPipeline      (device, post_process.pipeline,        nullptr);
	vkDestroyPipelineLayout(device, post_process.pipeline_layout, nullptr);

	vkDestroyFramebuffer(device, post_process.frame_buffer, nullptr);

	vkDestroySampler(device, post_process.sampler, nullptr);

	for (int i = 0; i < swapchain_views.size(); i++) {
		vkDestroyFramebuffer(device, frame_buffers[i], nullptr);
		vkDestroyImageView  (device, swapchain_views  [i], nullptr);
	}

	vkDestroySwapchainKHR(device, swapchain, nullptr);
	
	vkDestroyDescriptorPool(device, descriptor_pool_gui, nullptr);

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
}
