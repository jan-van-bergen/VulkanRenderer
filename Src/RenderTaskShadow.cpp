#include "RenderTaskShadow.h"

#include "VulkanCheck.h"
#include "VulkanContext.h"

#include "Scene.h"

struct ShadowPushConstants {
	alignas(16) Matrix4 wvp;
};

void RenderTaskShadow::init() {
	auto device = VulkanContext::get_device();

	auto depth_format = VulkanContext::get_supported_depth_format();
	
	// Create Descriptor Set Layout
	VkDescriptorSetLayoutCreateInfo layout_create_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	layout_create_info.bindingCount = 0;
	layout_create_info.pBindings    = nullptr;

	VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_create_info, nullptr, &descriptor_set_layout));

	// Create Render Pass
	VkAttachmentDescription depth_attachment = { };
	depth_attachment.format = depth_format;
	depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depth_attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	
	render_pass = VulkanContext::create_render_pass({ depth_attachment });

	// Create Shadow Map Render Target for each Light
	for (auto & directional_light : scene.directional_lights) {
		directional_light.shadow_map.render_target.add_attachment(SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT, depth_format,
			VkImageUsageFlagBits(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);
		directional_light.shadow_map.render_target.init(SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT, render_pass);
	}
	
	// Create Pipeline Layout
	std::vector<VkPushConstantRange> push_constants(1);
	push_constants[0].offset = 0;
	push_constants[0].size = sizeof(ShadowPushConstants);
	push_constants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VulkanContext::PipelineLayoutDetails pipeline_layout_details;
	pipeline_layout_details.descriptor_set_layouts = { descriptor_set_layout };
	pipeline_layout_details.push_constants = push_constants;

	pipeline_layout = VulkanContext::create_pipeline_layout(pipeline_layout_details);

	// Create Pipeline
	VulkanContext::PipelineDetails pipeline_details;
	pipeline_details.vertex_bindings   = Mesh::Vertex::get_binding_descriptions();
	pipeline_details.vertex_attributes = Mesh::Vertex::get_attribute_descriptions();
	pipeline_details.width  = SHADOW_MAP_WIDTH;
	pipeline_details.height = SHADOW_MAP_HEIGHT;
	pipeline_details.cull_mode = VK_CULL_MODE_BACK_BIT;
	pipeline_details.blends = { VulkanContext::PipelineDetails::BLEND_NONE };
	pipeline_details.shaders = { { "Shaders/shadow.vert.spv", VK_SHADER_STAGE_VERTEX_BIT } }; // NOTE: no Fragment Shader, we only care about depth
	pipeline_details.enable_depth_bias = true;
	pipeline_details.pipeline_layout = pipeline_layout;
	pipeline_details.render_pass     = render_pass;

	pipeline = VulkanContext::create_pipeline(pipeline_details);
}

void RenderTaskShadow::free() {
	auto device = VulkanContext::get_device();

	vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
	
	vkDestroyPipeline      (device, pipeline,        nullptr);
	vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
	
	vkDestroyRenderPass(device, render_pass, nullptr);
	
	for (auto & directional_light : scene.directional_lights) {
		directional_light.shadow_map.render_target.free();
	}
}

void RenderTaskShadow::render(int image_index, VkCommandBuffer command_buffer) {
	// Render to Shadow Maps
	for (auto const & directional_light : scene.directional_lights) {
		VkClearValue clear = { }; clear.depthStencil = { 1.0f, 0 };
		
		VkRenderPassBeginInfo render_pass_begin_info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		render_pass_begin_info.renderPass  = render_pass;
		render_pass_begin_info.framebuffer = directional_light.shadow_map.render_target.frame_buffer;
		render_pass_begin_info.renderArea.extent.width  = SHADOW_MAP_WIDTH;
		render_pass_begin_info.renderArea.extent.height = SHADOW_MAP_HEIGHT;
		render_pass_begin_info.clearValueCount = 1;
		render_pass_begin_info.pClearValues    = &clear;

		vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport = { 0.0f, 0.0f, float(SHADOW_MAP_WIDTH), float(SHADOW_MAP_HEIGHT), 0.0f, 1.0f };
		vkCmdSetViewport(command_buffer, 0, 1, &viewport);
	
		VkRect2D scissor = { 0, 0, SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT };
		vkCmdSetScissor(command_buffer, 0, 1, &scissor);

		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		for (int i = 0; i < scene.meshes.size(); i++) {
			auto const & mesh_instance = scene.meshes[i];
			auto const & mesh = Mesh::meshes[mesh_instance.mesh_handle];

			auto transform = mesh_instance.transform.matrix;
			
			ShadowPushConstants push_constants;
			push_constants.wvp = scene.directional_lights[0].get_light_matrix() * transform;

			vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowPushConstants), &push_constants);

			for (int j = 0; j < mesh.sub_meshes.size(); j++) {
				auto const & sub_mesh = mesh.sub_meshes[j];

				VkBuffer vertex_buffers[] = { sub_mesh.vertex_buffer.buffer };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);

				vkCmdBindIndexBuffer(command_buffer, sub_mesh.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

				vkCmdDrawIndexed(command_buffer, sub_mesh.index_count, 1, 0, 0, 0);
			}
		}

		vkCmdEndRenderPass(command_buffer);
	}
}