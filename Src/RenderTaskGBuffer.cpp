#include "RenderTaskGBuffer.h"

#include "VulkanCheck.h"
#include "VulkanContext.h"

#include "Matrix4.h"

#include "Mesh.h"
#include "Texture.h"

#include "Util.h"

struct GBufferPushConstants {
	alignas(16) Matrix4 world;
	union {
		alignas(16) Matrix4 wvp;
		alignas(16) Matrix4 view_projection;
	};
	alignas(4) int bone_offset;
};

struct MaterialUBO {
	alignas(4) float material_roughness;
	alignas(4) float material_metallic;
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

	// Create Descriptor Set Layouts
	{
		// Static Geometry
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
	}

	{
		// Material
		VkDescriptorSetLayoutBinding layout_bindings_material[1] = { };
		layout_bindings_material[0].binding = 0;
		layout_bindings_material[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		layout_bindings_material[0].descriptorCount = 1;
		layout_bindings_material[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		layout_bindings_material[0].pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo layout_create_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		layout_create_info.bindingCount = Util::array_element_count(layout_bindings_material);
		layout_create_info.pBindings    = layout_bindings_material;

		VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_create_info, nullptr, &descriptor_set_layouts.material));
	}
	
	{
		// Bones
		VkDescriptorSetLayoutBinding layout_bindings_geometry[1] = { };
		layout_bindings_geometry[0].binding = 0;
		layout_bindings_geometry[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		layout_bindings_geometry[0].descriptorCount = 1;
		layout_bindings_geometry[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		layout_bindings_geometry[0].pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo layout_create_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		layout_create_info.bindingCount = Util::array_element_count(layout_bindings_geometry);
		layout_create_info.pBindings    = layout_bindings_geometry;

		VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_create_info, nullptr, &descriptor_set_layouts.bones));
	}

	{
		// Sky
		VkDescriptorSetLayoutBinding layout_bindings_sky[1] = { };
		layout_bindings_sky[0].binding = 0;
		layout_bindings_sky[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		layout_bindings_sky[0].descriptorCount = 1;
		layout_bindings_sky[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		layout_bindings_sky[0].pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo layout_create_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		layout_create_info.bindingCount = Util::array_element_count(layout_bindings_sky);
		layout_create_info.pBindings    = layout_bindings_sky;

		VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_create_info, nullptr, &descriptor_set_layouts.sky));
	}

	// Initialize FrameBuffers and their attachments
	render_target.add_attachment(width, height, VK_FORMAT_R8G8B8A8_UNORM,                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT         | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL); // Albedo
	render_target.add_attachment(width, height, VK_FORMAT_R16G16B16A16_SFLOAT,               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT         | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL); // Normal (packed in xy) + Roughness + Metallic 
	render_target.add_attachment(width, height, VulkanContext::get_supported_depth_format(), VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL); // Depth

	render_pass = VulkanContext::create_render_pass(render_target.get_attachment_descriptions());
	render_target.init(width, height, render_pass);

	VkPushConstantRange push_constants;
	push_constants.offset = 0;
	push_constants.size = sizeof(GBufferPushConstants);
	push_constants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VulkanContext::PipelineLayoutDetails pipeline_layout_details;
	pipeline_layout_details.descriptor_set_layouts = {
		descriptor_set_layouts.geometry,
		descriptor_set_layouts.material
	};
	pipeline_layout_details.push_constants = { push_constants };

	pipeline_layouts.geometry_static = VulkanContext::create_pipeline_layout(pipeline_layout_details);

	pipeline_layout_details.descriptor_set_layouts = {
		descriptor_set_layouts.geometry,
		descriptor_set_layouts.material,
		descriptor_set_layouts.bones
	};

	pipeline_layouts.geometry_animated = VulkanContext::create_pipeline_layout(pipeline_layout_details);

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
		VulkanContext::PipelineDetails::BLEND_NONE
	};
	pipeline_details.cull_mode = VK_CULL_MODE_BACK_BIT;
	pipeline_details.shaders = {
		{ "Shaders/geometry_static.vert.spv", VK_SHADER_STAGE_VERTEX_BIT   },
		{ "Shaders/geometry.frag.spv",        VK_SHADER_STAGE_FRAGMENT_BIT }
	};
	pipeline_details.pipeline_layout = pipeline_layouts.geometry_static;
	pipeline_details.render_pass     = render_pass;

	pipelines.geometry_static = VulkanContext::create_pipeline(pipeline_details);

	pipeline_details.vertex_bindings   = AnimatedMesh::Vertex::get_binding_descriptions();
	pipeline_details.vertex_attributes = AnimatedMesh::Vertex::get_attribute_descriptions();
	pipeline_details.shaders = {
		{ "Shaders/geometry_animated.vert.spv", VK_SHADER_STAGE_VERTEX_BIT   },
		{ "Shaders/geometry.frag.spv",          VK_SHADER_STAGE_FRAGMENT_BIT }
	};
	pipeline_details.pipeline_layout = pipeline_layouts.geometry_animated;

	pipelines.geometry_animated = VulkanContext::create_pipeline(pipeline_details);

	pipeline_details.vertex_bindings   = { };
	pipeline_details.vertex_attributes = { };
	pipeline_details.blends[1].colorWriteMask = 0; // Don't write to normal buffer
	pipeline_details.cull_mode = VK_CULL_MODE_NONE;
	pipeline_details.shaders = {
		{ "Shaders/sky.vert.spv", VK_SHADER_STAGE_VERTEX_BIT },
		{ "Shaders/sky.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT }
	};
	pipeline_details.enable_depth_write = false;
	pipeline_details.depth_compare = VK_COMPARE_OP_EQUAL;
	pipeline_details.pipeline_layout = pipeline_layouts.sky;

	pipelines.sky = VulkanContext::create_pipeline(pipeline_details);

	// Create Uniform Buffers
	uniform_buffers.material.resize(swapchain_image_count);
	uniform_buffers.bones   .resize(swapchain_image_count);
	uniform_buffers.sky     .resize(swapchain_image_count);

	auto aligned_size_material = Math::round_up(sizeof(MaterialUBO), VulkanContext::get_min_uniform_buffer_alignment());
	auto aligned_size_sky      = Math::round_up(sizeof(SkyUBO),      VulkanContext::get_min_uniform_buffer_alignment());
	
	auto total_mesh_count = scene.animated_meshes.size() + scene.meshes.size();
	auto total_bone_count = 0;

	for (auto const & mesh_instance : scene.animated_meshes) total_bone_count += mesh_instance.get_mesh().bones.size();

	for (int i = 0; i < swapchain_image_count; i++) {
		uniform_buffers.material[i] = VulkanMemory::buffer_create(total_mesh_count * aligned_size_material,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		);
		
		uniform_buffers.bones[i] = VulkanMemory::buffer_create(total_bone_count * sizeof(Matrix4),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		);

		uniform_buffers.sky[i] = VulkanMemory::buffer_create(aligned_size_sky,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		);
	}

	// Allocate and update Descriptor Sets
	for (auto & texture : Texture::textures) {
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

	{
		std::vector<VkDescriptorSetLayout> layouts(swapchain_image_count, descriptor_set_layouts.material);

		VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		alloc_info.descriptorPool = descriptor_pool;
		alloc_info.descriptorSetCount = layouts.size();
		alloc_info.pSetLayouts        = layouts.data();

		descriptor_sets.material.resize(swapchain_image_count);
		VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, descriptor_sets.material.data()));

		for (int i = 0; i < descriptor_sets.material.size(); i++) {
			auto descriptor_set = descriptor_sets.material[i];

			VkDescriptorBufferInfo descriptor_ubo = { };
			descriptor_ubo.buffer = uniform_buffers.material[i].buffer;
			descriptor_ubo.offset = 0;
			descriptor_ubo.range = sizeof(MaterialUBO);

			VkWriteDescriptorSet write_descriptor_set = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			write_descriptor_set.dstSet = descriptor_set;
			write_descriptor_set.dstBinding = 0;
			write_descriptor_set.dstArrayElement = 0;
			write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			write_descriptor_set.descriptorCount = 1;
			write_descriptor_set.pBufferInfo     = &descriptor_ubo;

			vkUpdateDescriptorSets(device, 1, &write_descriptor_set, 0, nullptr);
		}
	}

	{
		std::vector<VkDescriptorSetLayout> layouts(swapchain_image_count, descriptor_set_layouts.bones);

		VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		alloc_info.descriptorPool = descriptor_pool;
		alloc_info.descriptorSetCount = layouts.size();
		alloc_info.pSetLayouts        = layouts.data();

		descriptor_sets.bones.resize(swapchain_image_count);
		VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, descriptor_sets.bones.data()));

		for (int i = 0; i < descriptor_sets.bones.size(); i++) {
			auto descriptor_set = descriptor_sets.bones[i];

			VkDescriptorBufferInfo buffer_info = { };
			buffer_info.buffer = uniform_buffers.bones[i].buffer;
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

	{
		std::vector<VkDescriptorSetLayout> layouts(swapchain_image_count, descriptor_set_layouts.sky);

		VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		alloc_info.descriptorPool = descriptor_pool;
		alloc_info.descriptorSetCount = layouts.size();
		alloc_info.pSetLayouts        = layouts.data();

		descriptor_sets.sky.resize(swapchain_image_count);
		VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, descriptor_sets.sky.data()));

		for (int i = 0; i < descriptor_sets.sky.size(); i++) {
			auto descriptor_set = descriptor_sets.sky[i];

			VkDescriptorBufferInfo descriptor_ubo = { };
			descriptor_ubo.buffer = uniform_buffers.sky[i].buffer;
			descriptor_ubo.offset = 0;
			descriptor_ubo.range = sizeof(SkyUBO);

			VkWriteDescriptorSet write_descriptor_set = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			write_descriptor_set.dstSet = descriptor_set;
			write_descriptor_set.dstBinding = 0;
			write_descriptor_set.dstArrayElement = 0;
			write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			write_descriptor_set.descriptorCount = 1;
			write_descriptor_set.pBufferInfo     = &descriptor_ubo;

			vkUpdateDescriptorSets(device, 1, &write_descriptor_set, 0, nullptr);
		}
	}
}

void RenderTaskGBuffer::free() {
	auto device = VulkanContext::get_device();

	render_target.free();
	
	vkDestroyDescriptorSetLayout(device, descriptor_set_layouts.geometry, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptor_set_layouts.material, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptor_set_layouts.bones,    nullptr);
	vkDestroyDescriptorSetLayout(device, descriptor_set_layouts.sky,      nullptr);
	
	vkDestroyPipelineLayout(device, pipeline_layouts.geometry_static,   nullptr);
	vkDestroyPipelineLayout(device, pipeline_layouts.geometry_animated, nullptr);
	vkDestroyPipelineLayout(device, pipeline_layouts.sky,               nullptr);

	vkDestroyPipeline(device, pipelines.geometry_static,   nullptr);
	vkDestroyPipeline(device, pipelines.geometry_animated, nullptr);
	vkDestroyPipeline(device, pipelines.sky,               nullptr);
	
	for (auto & uniform_buffer : uniform_buffers.material) VulkanMemory::buffer_free(uniform_buffer);
	for (auto & uniform_buffer : uniform_buffers.bones)    VulkanMemory::buffer_free(uniform_buffer);
	for (auto & uniform_buffer : uniform_buffers.sky)      VulkanMemory::buffer_free(uniform_buffer);

	vkDestroyRenderPass(device, render_pass, nullptr);
}

void RenderTaskGBuffer::render(int image_index, VkCommandBuffer command_buffer) {
	// Clear values for all attachments written in the fragment shader
	VkClearValue clear[3] = { };
	clear[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clear[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clear[2].depthStencil = { 1.0f, 0 };

	assert(Util::array_element_count(clear) == render_target.attachments.size());

	VkRenderPassBeginInfo render_pass_begin_info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	render_pass_begin_info.renderPass =  render_pass;
	render_pass_begin_info.framebuffer = render_target.frame_buffer;
	render_pass_begin_info.renderArea.extent.width  = width;
	render_pass_begin_info.renderArea.extent.height = height;
	render_pass_begin_info.clearValueCount = Util::array_element_count(clear);
	render_pass_begin_info.pClearValues    = clear;

	vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
	
	auto last_texture_handle = -1;

	auto & uniform_buffer_material = uniform_buffers.material[image_index];
	auto & descriptor_set_material = descriptor_sets.material[image_index];
	
	auto aligned_size = Math::round_up(sizeof(MaterialUBO), VulkanContext::get_min_uniform_buffer_alignment());
	auto total_mesh_count = scene.animated_meshes.size() + scene.meshes.size();

	std::vector<std::byte> buffer_material_ubo(total_mesh_count * aligned_size);
	std::vector<std::byte> buffer_bones;

	auto num_unculled_mesh_instances = 0;
	u32  bone_offset = 0;

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.geometry_animated);
	
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layouts.geometry_animated, 2, 1, &descriptor_sets.bones[image_index], 0, nullptr);

	for (int i = 0; i < scene.animated_meshes.size(); i++) {
		auto const & mesh_instance = scene.animated_meshes[i];
		auto const & mesh = mesh_instance.get_mesh();
		
		GBufferPushConstants push_constants = { };
		push_constants.world           = mesh_instance.transform.matrix;
		push_constants.view_projection = scene.camera.get_view_projection();
		push_constants.bone_offset = bone_offset;

		vkCmdPushConstants(command_buffer, pipeline_layouts.geometry_animated, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GBufferPushConstants), &push_constants);
				
		auto ubo = reinterpret_cast<MaterialUBO *>(&buffer_material_ubo[num_unculled_mesh_instances * aligned_size]);
		ubo->material_roughness = 0.9f;
		ubo->material_metallic  = 0.0f;

		u32 offset = num_unculled_mesh_instances * aligned_size;
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layouts.geometry_animated, 1, 1, &descriptor_set_material, 1, &offset);
		num_unculled_mesh_instances++;
				
		VkBuffer vertex_buffers[] = { mesh.vertex_buffer.buffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);

		vkCmdBindIndexBuffer(command_buffer, mesh.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		
		if (last_texture_handle != mesh.texture_handle) {
			last_texture_handle  = mesh.texture_handle;

			auto const & texture = Texture::textures[mesh.texture_handle];
			vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layouts.geometry_animated, 0, 1, &texture.descriptor_set, 0, nullptr);
		}

		buffer_bones.resize(buffer_bones.size() + mesh.bones.size() * sizeof(Matrix4));
		std::memcpy(buffer_bones.data() + bone_offset * sizeof(Matrix4), mesh_instance.bone_transforms.data(), mesh_instance.bone_transforms.size() * sizeof(Matrix4));

		bone_offset += mesh.bones.size();

		vkCmdDrawIndexed(command_buffer, mesh.index_count, 1, 0, 0, 0);
	}
	
	if (buffer_bones.size() > 0) VulkanMemory::buffer_copy_direct(uniform_buffers.bones[image_index], buffer_bones.data(), buffer_bones.size());

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.geometry_static);
	
	// Render Renderables
	for (int i = 0; i < scene.meshes.size(); i++) {
		auto const & mesh_instance = scene.meshes[i];
		auto const & mesh = mesh_instance.get_mesh();

		auto     transform = mesh_instance.transform.matrix;
		auto abs_transform = Matrix4::abs(transform);

		auto first_sub_mesh = true;

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

			// The first unculled Submesh must set the Push Constants, bind Descriptor Sets, and bind Vertex/Index Buffers
			if (first_sub_mesh) {		
				first_sub_mesh = false;

				GBufferPushConstants push_constants = { };
				push_constants.world = transform;
				push_constants.wvp   = scene.camera.get_view_projection() * transform;

				vkCmdPushConstants(command_buffer, pipeline_layouts.geometry_static, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GBufferPushConstants), &push_constants);
				
				auto ubo = reinterpret_cast<MaterialUBO *>(&buffer_material_ubo[num_unculled_mesh_instances * aligned_size]);
				ubo->material_roughness = mesh_instance.material.roughness;
				ubo->material_metallic  = mesh_instance.material.metallic;

				u32 offset = num_unculled_mesh_instances * aligned_size;
				vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layouts.geometry_static, 1, 1, &descriptor_set_material, 1, &offset);
				num_unculled_mesh_instances++;
				
				VkBuffer vertex_buffers[] = { mesh.vertex_buffer.buffer };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);

				vkCmdBindIndexBuffer(command_buffer, mesh.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
			}

			if (last_texture_handle != sub_mesh.texture_handle) {
				last_texture_handle  = sub_mesh.texture_handle;

				auto const & texture = Texture::textures[sub_mesh.texture_handle];
				vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layouts.geometry_static, 0, 1, &texture.descriptor_set, 0, nullptr);
			}
			
			vkCmdDrawIndexed(command_buffer, sub_mesh.index_count, 1, sub_mesh.index_offset, 0, 0);
		}
	}

	if (num_unculled_mesh_instances > 0) VulkanMemory::buffer_copy_direct(uniform_buffer_material, buffer_material_ubo.data(), num_unculled_mesh_instances * aligned_size);
	
	// Render Sky
	auto & uniform_buffer_sky = uniform_buffers.sky[image_index];
	auto & descriptor_set_sky = descriptor_sets.sky[image_index];

	vkCmdBindPipeline      (command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.sky);
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layouts.sky, 0, 1, &descriptor_set_sky, 0, nullptr);

	SkyUBO sky_ubo = { };
	sky_ubo.camera_top_left_corner = scene.camera.rotation * scene.camera.top_left_corner;
	sky_ubo.camera_x               = scene.camera.rotation * Vector3(float(width), 0.0f, 0.0f);
	sky_ubo.camera_y               = scene.camera.rotation * Vector3(0.0f, -float(height), 0.0f);
	sky_ubo.sun_direction = -scene.directional_lights[0].get_direction();

	VulkanMemory::buffer_copy_direct(uniform_buffer_sky, &sky_ubo, sizeof(sky_ubo));

	vkCmdDraw(command_buffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(command_buffer);
}
