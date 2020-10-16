#include "RenderTaskGBuffer.h"

#include "VulkanCheck.h"
#include "VulkanContext.h"

#include "Matrix4.h"

#include "Mesh.h"
#include "Texture.h"

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

void RenderTaskGBuffer::init(VkDescriptorPool descriptor_pool, int width, int height, int swapchain_image_count) {
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
	render_target.add_attachment(width, height, VK_FORMAT_R16G16B16A16_SFLOAT,               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	render_target.add_attachment(width, height, VK_FORMAT_R16G16B16A16_SFLOAT,               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	render_target.add_attachment(width, height, VK_FORMAT_R16G16_SFLOAT,                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	render_target.add_attachment(width, height, VulkanContext::get_supported_depth_format(), VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	render_pass = VulkanContext::create_render_pass(render_target.get_attachment_descriptions());
	render_target.init(width, height, render_pass);

	VkPushConstantRange push_constants;
	push_constants.offset = 0;
	push_constants.size = sizeof(GBufferPushConstants);
	push_constants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VulkanContext::PipelineLayoutDetails pipeline_layout_details;
	pipeline_layout_details.descriptor_set_layouts = { descriptor_set_layouts.geometry };
	pipeline_layout_details.push_constants = { push_constants };

	pipeline_layouts.geometry = VulkanContext::create_pipeline_layout(pipeline_layout_details);

	pipeline_layout_details.descriptor_set_layouts = { descriptor_set_layouts.sky };
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
	pipeline_details.shaders = {
		{ "Shaders/geometry.vert.spv", VK_SHADER_STAGE_VERTEX_BIT   },
		{ "Shaders/geometry.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT }
	};
	pipeline_details.pipeline_layout = pipeline_layouts.geometry;
	pipeline_details.render_pass     = render_pass;

	pipelines.geometry = VulkanContext::create_pipeline(pipeline_details);

	pipeline_details.vertex_bindings   = { };
	pipeline_details.vertex_attributes = { };
	pipeline_details.blends[1].colorWriteMask = 0; // Don't write to position buffer
	pipeline_details.blends[2].colorWriteMask = 0; // Don't write to normal   buffer
	pipeline_details.cull_mode = VK_CULL_MODE_FRONT_BIT;
	pipeline_details.shaders = {
		{ "Shaders/sky.vert.spv", VK_SHADER_STAGE_VERTEX_BIT },
		{ "Shaders/sky.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT }
	};
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

	descriptor_sets_sky.resize(swapchain_image_count);
	VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, descriptor_sets_sky.data()));

	for (int i = 0; i < descriptor_sets_sky.size(); i++) {
		auto & descriptor_set = descriptor_sets_sky[i];

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
}

void RenderTaskGBuffer::free() {
	auto device = VulkanContext::get_device();

	render_target.free();
	
	vkDestroyDescriptorSetLayout(device, descriptor_set_layouts.geometry, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptor_set_layouts.sky,      nullptr);
	
	vkDestroyPipeline      (device, pipelines.geometry, nullptr);
	vkDestroyPipeline      (device, pipelines.sky,      nullptr);
	vkDestroyPipelineLayout(device, pipeline_layouts.geometry, nullptr);
	vkDestroyPipelineLayout(device, pipeline_layouts.sky,      nullptr);
	
	for (auto & uniform_buffer : uniform_buffers) {
		VulkanMemory::buffer_free(uniform_buffer);
	}
	
	vkDestroyRenderPass(device, render_pass, nullptr);
}

void RenderTaskGBuffer::render(int image_index, VkCommandBuffer command_buffer) {
	// Clear values for all attachments written in the fragment shader
	VkClearValue clear_gbuffer[4] = { };
	clear_gbuffer[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clear_gbuffer[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clear_gbuffer[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clear_gbuffer[3].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo render_pass_begin_info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	render_pass_begin_info.renderPass =  render_pass;
	render_pass_begin_info.framebuffer = render_target.frame_buffer;
	render_pass_begin_info.renderArea.extent.width  = width;
	render_pass_begin_info.renderArea.extent.height = height;
	render_pass_begin_info.clearValueCount = Util::array_element_count(clear_gbuffer);
	render_pass_begin_info.pClearValues    = clear_gbuffer;

	vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport = { 0.0f, 0.0f, float(width), float(height), 0.0f, 1.0f };

	VkRect2D scissor = { 0, 0, width, height };
	
	vkCmdSetViewport(command_buffer, 0, 1, &viewport);
	vkCmdSetScissor (command_buffer, 0, 1, &scissor);

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.geometry);

	auto last_texture_handle = -1;

	// Render Renderables
	for (int i = 0; i < scene.meshes.size(); i++) {
		auto const & mesh_instance = scene.meshes[i];
		auto const & mesh = Mesh::meshes[mesh_instance.mesh_handle];

		auto     transform = mesh_instance.transform.matrix;
		auto abs_transform = Matrix4::abs(transform);

		bool first_sub_mesh = true;

		for (int j = 0; j < mesh.sub_meshes.size(); j++) {
			auto const & sub_mesh = mesh.sub_meshes[j];

			// Transform AABB into world space for culling
			Vector3 center = 0.5f * (sub_mesh.aabb.min + sub_mesh.aabb.max);
			Vector3 extent = 0.5f * (sub_mesh.aabb.max - sub_mesh.aabb.min);

			Vector3 new_center = Matrix4::transform_position (    transform, center);
			Vector3 new_extent = Matrix4::transform_direction(abs_transform, extent);

			Vector3 aabb_world_min = new_center - new_extent;
			Vector3 aabb_world_max = new_center + new_extent;

			if (scene.camera.frustum.intersect_aabb(aabb_world_min, aabb_world_max) == Frustum::IntersectionType::FULLY_OUTSIDE) continue;

			// The first unculled Submesh must set the Push Constants
			if (first_sub_mesh) {		
				first_sub_mesh = false;

				GBufferPushConstants push_constants;
				push_constants.world = transform;
				push_constants.wvp   = scene.camera.get_view_projection() * transform;

				vkCmdPushConstants(command_buffer, pipeline_layouts.geometry, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GBufferPushConstants), &push_constants);
			}

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
	auto & descriptor_set = descriptor_sets_sky[image_index];

	vkCmdBindPipeline      (command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.sky);
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layouts.sky, 0, 1, &descriptor_set, 0, nullptr);

	SkyUBO sky_ubo = { };
	sky_ubo.camera_top_left_corner = scene.camera.rotation * scene.camera.top_left_corner;
	sky_ubo.camera_x               = scene.camera.rotation * Vector3(float(width), 0.0f, 0.0f);
	sky_ubo.camera_y               = scene.camera.rotation * Vector3(0.0f, -float(height), 0.0f);
	sky_ubo.sun_direction = -scene.directional_lights[0].direction;

	VulkanMemory::buffer_copy_direct(uniform_buffer, &sky_ubo, sizeof(sky_ubo));

	vkCmdDraw(command_buffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(command_buffer);
}
