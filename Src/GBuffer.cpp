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

struct SkyUBO {
	alignas(16) Vector3 camera_top_left_corner;
	alignas(16)	Vector3 camera_x;
	alignas(16)	Vector3 camera_y;

	alignas(16) Vector3 sun_direction;
};

void GBuffer::init(int swapchain_image_count, int width, int height) {
	auto device = VulkanContext::get_device();

	this->width  = width;
	this->height = height;

	// Create Descriptor Set Layout
	VkDescriptorSetLayoutBinding layout_bindings_geometry[1] = { };
	layout_bindings_geometry[0].binding = 1;
	layout_bindings_geometry[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	layout_bindings_geometry[0].descriptorCount = 1;
	layout_bindings_geometry[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	layout_bindings_geometry[0].pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layout_create_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	layout_create_info.bindingCount = Util::array_element_count(layout_bindings_geometry);
	layout_create_info.pBindings    = layout_bindings_geometry;

	VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_create_info, nullptr, &descriptor_set_layouts.geometry));
	
	VkDescriptorSetLayoutBinding layout_bindings_sky[1] = { };
	layout_bindings_sky[0].binding = 0;
	layout_bindings_sky[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	layout_bindings_sky[0].descriptorCount = 1;
	layout_bindings_sky[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	layout_bindings_sky[0].pImmutableSamplers = nullptr;

	layout_create_info.bindingCount = Util::array_element_count(layout_bindings_sky);
	layout_create_info.pBindings    = layout_bindings_sky;

	VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_create_info, nullptr, &descriptor_set_layouts.sky));

	// Create Descriptor Pool
	VkDescriptorPoolSize descriptor_pool_sizes[] = {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Texture::textures.size() },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         swapchain_image_count }
	};

	VkDescriptorPoolCreateInfo pool_create_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pool_create_info.poolSizeCount = Util::array_element_count(descriptor_pool_sizes);
	pool_create_info.pPoolSizes    = descriptor_pool_sizes;
	pool_create_info.maxSets = Texture::textures.size() + swapchain_image_count;

	VK_CHECK(vkCreateDescriptorPool(device, &pool_create_info, nullptr, &descriptor_pool));

	// Allocate and update Texture Descriptor Sets
	for (int i = 0; i < Texture::textures.size(); i++) {
		auto & texture = Texture::textures[i];

		VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		alloc_info.descriptorPool = descriptor_pool;
		alloc_info.descriptorSetCount = 1;
		alloc_info.pSetLayouts        = &descriptor_set_layouts.geometry;

		VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, &texture.descriptor_set));

		VkDescriptorImageInfo image_info;
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		image_info.imageView = texture.image_view;
		image_info.sampler   = texture.sampler;

		VkWriteDescriptorSet write_descriptor_set = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write_descriptor_set.dstSet = texture.descriptor_set;
		write_descriptor_set.dstBinding = 1;
		write_descriptor_set.dstArrayElement = 0;
		write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write_descriptor_set.descriptorCount = 1;
		write_descriptor_set.pImageInfo      = &image_info;

		vkUpdateDescriptorSets(device, 1, &write_descriptor_set, 0, nullptr);
	}

	// Initialize FrameBuffers and their attachments
	render_target_albedo  .init(width, height, VK_FORMAT_R16G16B16A16_SFLOAT,               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
	render_target_position.init(width, height, VK_FORMAT_R16G16B16A16_SFLOAT,               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
	render_target_normal  .init(width, height, VK_FORMAT_R16G16_SFLOAT,                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
	render_target_depth   .init(width, height, VulkanContext::get_supported_depth_format(), VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

	// Create Renderpass
	VkAttachmentDescription attachments[4] = { };
	attachments[0].format = render_target_albedo  .format;
	attachments[1].format = render_target_position.format;
	attachments[2].format = render_target_normal  .format;
	attachments[3].format = render_target_depth   .format;

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
	
	VkPushConstantRange push_constants;
	push_constants.offset = 0;
	push_constants.size = sizeof(GBufferPushConstants);
	push_constants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VulkanContext::PipelineLayoutDetails pipeline_layout_details;
	pipeline_layout_details.descriptor_set_layout = descriptor_set_layouts.geometry;
	pipeline_layout_details.push_constants = { push_constants };

	pipeline_layouts.geometry = VulkanContext::create_pipeline_layout(pipeline_layout_details);

	pipeline_layout_details.descriptor_set_layout = descriptor_set_layouts.sky;
	pipeline_layout_details.push_constants = { };

	pipeline_layouts.sky = VulkanContext::create_pipeline_layout(pipeline_layout_details);

	// Create Pipeline
	VulkanContext::PipelineDetails pipeline_details;
	pipeline_details.vertex_bindings   = Mesh::Vertex::get_binding_descriptions();
	pipeline_details.vertex_attributes = Mesh::Vertex::get_attribute_descriptions();
	pipeline_details.width  = width;
	pipeline_details.height = height;
	pipeline_details.blends = {
		VulkanContext::PipelineDetails::BLEND_NONE,
		VulkanContext::PipelineDetails::BLEND_NONE,
		VulkanContext::PipelineDetails::BLEND_NONE
	};
	pipeline_details.cull_mode = VK_CULL_MODE_BACK_BIT;
	pipeline_details.filename_shader_vertex   = "Shaders/geometry.vert.spv";
	pipeline_details.filename_shader_fragment = "Shaders/geometry.frag.spv";
	pipeline_details.pipeline_layout = pipeline_layouts.geometry;
	pipeline_details.render_pass     = render_pass;

	pipelines.geometry = VulkanContext::create_pipeline(pipeline_details);

	pipeline_details.vertex_bindings   = { };
	pipeline_details.vertex_attributes = { };
	pipeline_details.blends[1].colorWriteMask = 0; // Don't write to position buffer
	pipeline_details.blends[2].colorWriteMask = 0; // Don't write to normal   buffer
	pipeline_details.cull_mode = VK_CULL_MODE_FRONT_BIT;
	pipeline_details.filename_shader_vertex   = "Shaders/sky.vert.spv";
	pipeline_details.filename_shader_fragment = "Shaders/sky.frag.spv";
	pipeline_details.enable_depth_write = false;
	pipeline_details.depth_compare = VK_COMPARE_OP_EQUAL;
	pipeline_details.pipeline_layout = pipeline_layouts.sky;

	pipelines.sky = VulkanContext::create_pipeline(pipeline_details);

	// Create Uniform Buffers
	uniform_buffers.resize(swapchain_image_count);

	auto aligned_size = Math::round_up(sizeof(SkyUBO), VulkanContext::get_min_uniform_buffer_alignment());
	
	for (int i = 0; i < swapchain_image_count; i++) {
		uniform_buffers[i] = VulkanMemory::buffer_create(aligned_size,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		);
	}

	// Allocate and update Descriptor Sets
	std::vector<VkDescriptorSetLayout> layouts(swapchain_image_count, descriptor_set_layouts.sky);

	VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = layouts.size();
	alloc_info.pSetLayouts        = layouts.data();

	descriptor_sets.resize(swapchain_image_count);
	VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, descriptor_sets.data()));

	for (int i = 0; i < descriptor_sets.size(); i++) {
		auto & descriptor_set = descriptor_sets[i];

		VkWriteDescriptorSet write_descriptor_sets[1] = { };

		VkDescriptorBufferInfo descriptor_ubo = { };
		descriptor_ubo.buffer = uniform_buffers[i].buffer;
		descriptor_ubo.offset = 0;
		descriptor_ubo.range = sizeof(SkyUBO);

		write_descriptor_sets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_descriptor_sets[0].dstSet = descriptor_set;
		write_descriptor_sets[0].dstBinding = 0;
		write_descriptor_sets[0].dstArrayElement = 0;
		write_descriptor_sets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		write_descriptor_sets[0].descriptorCount = 1;
		write_descriptor_sets[0].pBufferInfo     = &descriptor_ubo;

		vkUpdateDescriptorSets(device, Util::array_element_count(write_descriptor_sets), write_descriptor_sets, 0, nullptr);
	}

	// Create Frame Buffer
	frame_buffer = VulkanContext::create_frame_buffer(width, height, render_pass, {
		render_target_albedo  .image_view,
		render_target_position.image_view,
		render_target_normal  .image_view,
		render_target_depth   .image_view,
	});

	// Create Command Buffers
	command_buffers.resize(swapchain_image_count);

	VkCommandBufferAllocateInfo command_buffer_alloc_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	command_buffer_alloc_info.commandPool = VulkanContext::get_command_pool();
	command_buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	command_buffer_alloc_info.commandBufferCount = command_buffers.size();

	VK_CHECK(vkAllocateCommandBuffers(device, &command_buffer_alloc_info, command_buffers.data()));
}

void GBuffer::free() {
	auto device = VulkanContext::get_device();

	vkDestroyFramebuffer(device, frame_buffer, nullptr);

	render_target_albedo  .free();
	render_target_position.free();
	render_target_normal  .free();
	render_target_depth   .free();
	
	vkDestroyDescriptorPool     (device, descriptor_pool,                 nullptr);
	vkDestroyDescriptorSetLayout(device, descriptor_set_layouts.geometry, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptor_set_layouts.sky,      nullptr);

	vkDestroyPipeline      (device, pipelines.geometry, nullptr);
	vkDestroyPipeline      (device, pipelines.sky,      nullptr);
	vkDestroyPipelineLayout(device, pipeline_layouts.geometry, nullptr);
	vkDestroyPipelineLayout(device, pipeline_layouts.sky,      nullptr);
	
	for (auto & uniform_buffer : uniform_buffers) {
		VulkanMemory::buffer_free(uniform_buffer);
	}

	vkFreeCommandBuffers(device, VulkanContext::get_command_pool(), command_buffers.size(), command_buffers.data());

	vkDestroyRenderPass(device, render_pass, nullptr);
}

void GBuffer::record_command_buffer(int image_index, Camera const & camera, std::vector<MeshInstance> const & meshes, Vector3 const & sun_direction) {
	auto & command_buffer = command_buffers[image_index];

	VkCommandBufferBeginInfo command_buffer_begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

	// Clear values for all attachments written in the fragment shader
	VkClearValue clear_values[4] = { };
	clear_values[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clear_values[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clear_values[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clear_values[3].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo render_pass_begin_info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	render_pass_begin_info.renderPass =  render_pass;
	render_pass_begin_info.framebuffer = frame_buffer;
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

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.geometry);

	auto last_texture_handle = -1;

	// Render Renderables
	for (int i = 0; i < meshes.size(); i++) {
		auto const & mesh_instance = meshes[i];
		auto const & mesh = Mesh::meshes[mesh_instance.mesh_handle];

		auto transform = mesh_instance.transform.get_matrix();

		GBufferPushConstants push_constants;
		push_constants.world = transform;
		push_constants.wvp   = camera.get_view_projection() * transform;

		vkCmdPushConstants(command_buffer, pipeline_layouts.geometry, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GBufferPushConstants), &push_constants);

		for (int j = 0; j < mesh.sub_meshes.size(); j++) {
			auto const & sub_mesh = mesh.sub_meshes[j];

			VkBuffer vertex_buffers[] = { sub_mesh.vertex_buffer.buffer };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);

			vkCmdBindIndexBuffer(command_buffer, sub_mesh.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

			if (last_texture_handle != sub_mesh.texture_handle) {
				last_texture_handle  = sub_mesh.texture_handle;

				auto const & texture = Texture::textures[sub_mesh.texture_handle];
				vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layouts.geometry, 0, 1, &texture.descriptor_set, 0, nullptr);
			}

			vkCmdDrawIndexed(command_buffer, sub_mesh.index_count, 1, 0, 0, 0);
		}
	}
	
	// Render Sky
	auto & uniform_buffer = uniform_buffers[image_index];
	auto & descriptor_set = descriptor_sets[image_index];

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.sky);

	SkyUBO sky_ubo = { };
	sky_ubo.camera_top_left_corner = camera.rotation * camera.top_left_corner;
	sky_ubo.camera_x               = camera.rotation * Vector3(float(width), 0.0f, 0.0f);
	sky_ubo.camera_y               = camera.rotation * Vector3(0.0f, -float(height), 0.0f);
	sky_ubo.sun_direction = -sun_direction;

	VulkanMemory::buffer_copy_direct(uniform_buffer, &sky_ubo, sizeof(sky_ubo));

	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layouts.sky, 0, 1, &descriptor_set, 0, nullptr);

	vkCmdDraw(command_buffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(command_buffer);

	VK_CHECK(vkEndCommandBuffer(command_buffer));
}
