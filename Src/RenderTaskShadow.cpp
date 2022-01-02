#include "RenderTaskShadow.h"

#include "VulkanCheck.h"
#include "VulkanContext.h"

#include "Scene.h"

struct ShadowPushConstants {
	alignas(16) Matrix4 wvp;
	alignas(4)  int bone_offset;
};

void RenderTaskShadow::init(VkDescriptorPool descriptor_pool, int swapchain_image_count) {
	auto device = VulkanContext::get_device();

	auto depth_format = VulkanContext::get_supported_depth_format();

	// Create Descriptor Set Layout
	VkDescriptorSetLayoutCreateInfo layout_create_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	layout_create_info.bindingCount = 0;
	layout_create_info.pBindings    = nullptr;

	VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_create_info, nullptr, &descriptor_set_layouts.shadow_static));

	VkDescriptorSetLayoutBinding bindings[1] = { };
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	bindings[0].pImmutableSamplers = nullptr;

	layout_create_info.bindingCount = Util::array_element_count(bindings);
	layout_create_info.pBindings    = bindings;

	VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_create_info, nullptr, &descriptor_set_layouts.shadow_animated));

	// Create Render Pass
	VkAttachmentDescription depth_attachment = { };
	depth_attachment.format = depth_format;
	depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depth_attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	render_pass = VulkanContext::create_render_pass({ depth_attachment });

	// Create Shadow Map Render Target for each Light
	for (auto & directional_light : scene.directional_lights) {
		directional_light.shadow_map.render_target.add_attachment(SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT, depth_format,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VulkanContext::create_clear_value_depth()
		);
		directional_light.shadow_map.render_target.init(SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT, render_pass, VK_FILTER_LINEAR);
	}

	// Create Pipeline Layout
	std::vector<VkPushConstantRange> push_constants(1);
	push_constants[0].offset = 0;
	push_constants[0].size = sizeof(ShadowPushConstants);
	push_constants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VulkanContext::PipelineLayoutDetails pipeline_layout_details;
	pipeline_layout_details.descriptor_set_layouts = { descriptor_set_layouts.shadow_static };
	pipeline_layout_details.push_constants = push_constants;

	pipeline_layouts.shadow_static = VulkanContext::create_pipeline_layout(pipeline_layout_details);

	pipeline_layout_details.descriptor_set_layouts = { descriptor_set_layouts.shadow_static, descriptor_set_layouts.shadow_animated };

	pipeline_layouts.shadow_animated = VulkanContext::create_pipeline_layout(pipeline_layout_details);

	// Create Pipeline
	VulkanContext::PipelineDetails pipeline_details;
	pipeline_details.vertex_bindings   = Mesh::Vertex::get_binding_descriptions();
	pipeline_details.vertex_attributes = Mesh::Vertex::get_attribute_descriptions();
	pipeline_details.width  = SHADOW_MAP_WIDTH;
	pipeline_details.height = SHADOW_MAP_HEIGHT;
	pipeline_details.cull_mode = VK_CULL_MODE_BACK_BIT;
	pipeline_details.blends = { VulkanContext::PipelineDetails::BLEND_NONE };
	pipeline_details.shaders = { { "Shaders/shadow_static.vert.spv", VK_SHADER_STAGE_VERTEX_BIT } }; // NOTE: no Fragment Shader, we only care about depth
	pipeline_details.enable_depth_bias = true;
	pipeline_details.pipeline_layout = pipeline_layouts.shadow_static;
	pipeline_details.render_pass     = render_pass;

	pipelines.shadow_static = VulkanContext::create_pipeline(pipeline_details);

	pipeline_details.vertex_bindings   = AnimatedMesh::Vertex::get_binding_descriptions();
	pipeline_details.vertex_attributes = AnimatedMesh::Vertex::get_attribute_descriptions();
	pipeline_details.shaders = { { "Shaders/shadow_animated.vert.spv", VK_SHADER_STAGE_VERTEX_BIT } }; // NOTE: no Fragment Shader, we only care about depth
	pipeline_details.pipeline_layout = pipeline_layouts.shadow_animated;

	pipelines.shadow_animated = VulkanContext::create_pipeline(pipeline_details);

	// Allocate and update Descriptor Sets
	auto total_bone_count = 0;

	for (auto const & mesh_instance : scene.animated_meshes) {
		total_bone_count += scene.asset_manager.get_animated_mesh(mesh_instance.mesh_handle).bones.size();
	}

	std::vector<VkDescriptorSetLayout> layouts(swapchain_image_count, descriptor_set_layouts.shadow_animated);

	VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = layouts.size();
	alloc_info.pSetLayouts        = layouts.data();

	descriptor_sets.bones.resize(swapchain_image_count);
	VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, descriptor_sets.bones.data()));

	for (int i = 0; i < descriptor_sets.bones.size(); i++) {
		auto descriptor_set = descriptor_sets.bones[i];

		VkDescriptorBufferInfo buffer_info = { };
		buffer_info.buffer = scene.asset_manager.storage_buffer_bones[i].buffer;
		buffer_info.offset = 0;
		buffer_info.range  = total_bone_count * sizeof(Matrix4);

		VkWriteDescriptorSet write_descriptor_set = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write_descriptor_set.dstSet = descriptor_set;
		write_descriptor_set.dstBinding = 0;
		write_descriptor_set.dstArrayElement = 0;
		write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		write_descriptor_set.descriptorCount = 1;
		write_descriptor_set.pBufferInfo     = &buffer_info;

		vkUpdateDescriptorSets(device, 1, &write_descriptor_set, 0, nullptr);
	}
}

void RenderTaskShadow::free() {
	auto device = VulkanContext::get_device();

	vkDestroyDescriptorSetLayout(device, descriptor_set_layouts.shadow_static,   nullptr);
	vkDestroyDescriptorSetLayout(device, descriptor_set_layouts.shadow_animated, nullptr);

	vkDestroyPipelineLayout(device, pipeline_layouts.shadow_static,   nullptr);
	vkDestroyPipelineLayout(device, pipeline_layouts.shadow_animated, nullptr);

	vkDestroyPipeline(device, pipelines.shadow_static,   nullptr);
	vkDestroyPipeline(device, pipelines.shadow_animated, nullptr);

	vkDestroyRenderPass(device, render_pass, nullptr);

	for (auto & directional_light : scene.directional_lights) {
		directional_light.shadow_map.render_target.free();
	}
}

void RenderTaskShadow::render(int image_index, VkCommandBuffer command_buffer) {
	for (auto const & directional_light : scene.directional_lights) {
		VkRenderPassBeginInfo render_pass_begin_info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		render_pass_begin_info.renderPass  = render_pass;
		render_pass_begin_info.framebuffer = directional_light.shadow_map.render_target.frame_buffer;
		render_pass_begin_info.renderArea.extent.width  = SHADOW_MAP_WIDTH;
		render_pass_begin_info.renderArea.extent.height = SHADOW_MAP_HEIGHT;
		render_pass_begin_info.clearValueCount = directional_light.shadow_map.render_target.clear_values.size();
		render_pass_begin_info.pClearValues    = directional_light.shadow_map.render_target.clear_values.data();

		vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.shadow_animated);

		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layouts.shadow_animated, 1, 1, &descriptor_sets.bones[image_index], 0, nullptr);

		int bone_offset = 0;

		for (int i = 0; i < scene.animated_meshes.size(); i++) {
			auto const & mesh_instance = scene.animated_meshes[i];
			auto const & mesh          = scene.asset_manager.get_animated_mesh(mesh_instance.mesh_handle);

			auto const & transform = mesh_instance.transform.matrix;

			ShadowPushConstants push_constants = { };
			push_constants.wvp = scene.directional_lights[0].get_light_matrix() * transform;
			push_constants.bone_offset = bone_offset;

			bone_offset += mesh.bones.size();

			vkCmdPushConstants(command_buffer, pipeline_layouts.shadow_animated, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowPushConstants), &push_constants);

			VkBuffer vertex_buffers[] = { mesh.vertex_buffer.buffer };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);

			vkCmdBindIndexBuffer(command_buffer, mesh.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

			for (int j = 0; j < mesh.sub_meshes.size(); j++) {
				auto const & sub_mesh = mesh.sub_meshes[j];

				vkCmdDrawIndexed(command_buffer, sub_mesh.index_count, 1, sub_mesh.index_offset, 0, 0);
			}
		}

		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.shadow_static);

		for (int i = 0; i < scene.meshes.size(); i++) {
			auto const & mesh_instance = scene.meshes[i];
			auto const & mesh = scene.asset_manager.get_mesh(mesh_instance.mesh_handle);

			auto const & transform = mesh_instance.transform.matrix;

			ShadowPushConstants push_constants = { };
			push_constants.wvp = scene.directional_lights[0].get_light_matrix() * transform;

			vkCmdPushConstants(command_buffer, pipeline_layouts.shadow_static, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowPushConstants), &push_constants);

			VkBuffer     vertex_buffers[] = { mesh.vertex_buffer.buffer };
			VkDeviceSize vertex_offsets[] = { 0 };
			vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, vertex_offsets);

			vkCmdBindIndexBuffer(command_buffer, mesh.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

			for (int j = 0; j < mesh.sub_meshes.size(); j++) {
				auto const & sub_mesh = mesh.sub_meshes[j];

				vkCmdDrawIndexed(command_buffer, sub_mesh.index_count, 1, sub_mesh.index_offset, 0, 0);
			}
		}

		vkCmdEndRenderPass(command_buffer);
	}
}