#include "RenderTaskGBuffer.h"

#include "VulkanCheck.h"
#include "VulkanContext.h"

#include "Vector4.h"
#include "Matrix4.h"

#include "Mesh.h"
#include "Texture.h"

#include "Util.h"

// Same layout as VkDrawIndexedIndirectCommand
struct IndexedIndirectCommand {
	unsigned index_count;
	unsigned instance_count;
	unsigned first_index;
	unsigned vertex_offset;
	unsigned first_instance;
};

struct Stats {
	unsigned draw_count;
};

struct Model {
	Vector3  position;
	unsigned index_count;
};

struct GBufferPushConstants {
	alignas(16) Matrix4 world;
	union {
		alignas(16) Matrix4 wvp;
		alignas(16) Matrix4 view_projection;
	};
	alignas(4) int bone_offset;
};

struct CameraUBO {
	alignas(16) Vector4 frustum_planes[6];
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
		// Compute Culling
		VkDescriptorSetLayoutBinding layout_bindings_cull[4] = { };

		layout_bindings_cull[0].binding = 0;
		layout_bindings_cull[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		layout_bindings_cull[0].descriptorCount = 1;
		layout_bindings_cull[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		
		layout_bindings_cull[1].binding = 1;
		layout_bindings_cull[1].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		layout_bindings_cull[1].descriptorCount = 1;
		layout_bindings_cull[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
			;
		layout_bindings_cull[2].binding = 2;
		layout_bindings_cull[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		layout_bindings_cull[2].descriptorCount = 1;
		layout_bindings_cull[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		layout_bindings_cull[3].binding = 3;
		layout_bindings_cull[3].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		layout_bindings_cull[3].descriptorCount = 1;
		layout_bindings_cull[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		VkDescriptorSetLayoutCreateInfo layout_create_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		layout_create_info.bindingCount = Util::array_element_count(layout_bindings_cull);
		layout_create_info.pBindings    = layout_bindings_cull;

		VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_create_info, nullptr, &descriptor_set_layouts.cull));
	}

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
	constexpr auto attachment_colour = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT         | VK_IMAGE_USAGE_SAMPLED_BIT;
	constexpr auto attachment_depth  = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	constexpr auto readonly = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	render_target.add_attachment(width, height, VK_FORMAT_R8G8B8A8_UNORM,                    attachment_colour, readonly, VulkanContext::create_clear_value_colour()); // Albedo
	render_target.add_attachment(width, height, VK_FORMAT_R16G16B16A16_SFLOAT,               attachment_colour, readonly, VulkanContext::create_clear_value_colour()); // Normal (packed in xy) + Roughness + Metallic 
	render_target.add_attachment(width, height, VulkanContext::get_supported_depth_format(), attachment_depth,  readonly, VulkanContext::create_clear_value_depth());  // Depth

	render_pass = VulkanContext::create_render_pass(render_target.get_attachment_descriptions());
	render_target.init(width, height, render_pass);

	VkPushConstantRange push_constants;
	push_constants.offset = 0;
	push_constants.size = sizeof(GBufferPushConstants);
	push_constants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VulkanContext::PipelineLayoutDetails pipeline_layout_details;

	// Cull Pipeline Layout
	pipeline_layout_details.descriptor_set_layouts = {
		descriptor_set_layouts.cull
	};
	pipeline_layout_details.push_constants = { };

	pipeline_layouts.cull = VulkanContext::create_pipeline_layout(pipeline_layout_details);
	
	// Static Geometry Pipeline Layout
	pipeline_layout_details.descriptor_set_layouts = {
		descriptor_set_layouts.geometry,
		descriptor_set_layouts.material
	};
	pipeline_layout_details.push_constants = { push_constants };

	pipeline_layouts.geometry_static = VulkanContext::create_pipeline_layout(pipeline_layout_details);

	// Animated Geometry Pipeline Layout
	pipeline_layout_details.descriptor_set_layouts = {
		descriptor_set_layouts.geometry,
		descriptor_set_layouts.material,
		descriptor_set_layouts.bones
	};

	pipeline_layouts.geometry_animated = VulkanContext::create_pipeline_layout(pipeline_layout_details);

	// Sky Pipeline Layout
	pipeline_layout_details.descriptor_set_layouts = { descriptor_set_layouts.sky };
	pipeline_layout_details.push_constants = { };

	pipeline_layouts.sky = VulkanContext::create_pipeline_layout(pipeline_layout_details);

	// Create Pipelines
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
	uniform_buffers.material.reserve(swapchain_image_count);
	uniform_buffers.sky     .reserve(swapchain_image_count);
	storage_buffers.cull_commands.reserve(swapchain_image_count);
	scene.asset_manager.storage_buffer_bones.reserve(swapchain_image_count);

	auto aligned_size_camera       = Math::round_up(sizeof(CameraUBO),              VulkanContext::get_min_uniform_buffer_alignment());
	auto aligned_size_material     = Math::round_up(sizeof(MaterialUBO),            VulkanContext::get_min_uniform_buffer_alignment());
	auto aligned_size_sky          = Math::round_up(sizeof(SkyUBO),                 VulkanContext::get_min_uniform_buffer_alignment());
	auto aligned_size_cull_command = Math::round_up(sizeof(IndexedIndirectCommand), VulkanContext::get_min_uniform_buffer_alignment());
	auto aligned_size_cull_stats   = Math::round_up(sizeof(Stats),                  VulkanContext::get_min_uniform_buffer_alignment());
	auto aligned_size_cull_model   = Math::round_up(sizeof(Model),                  VulkanContext::get_min_uniform_buffer_alignment());

	auto total_mesh_count = scene.animated_meshes.size() + scene.meshes.size();
	auto total_bone_count = 0;

	for (auto const & mesh_instance : scene.animated_meshes) {
		total_bone_count += scene.asset_manager.get_animated_mesh(mesh_instance.mesh_handle).bones.size();
	}

	for (int i = 0; i < swapchain_image_count; i++) {
		// Uniform Buffers
		uniform_buffers.camera.push_back(VulkanMemory::Buffer(aligned_size_camera,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		));

		uniform_buffers.material.push_back(VulkanMemory::Buffer(total_mesh_count * aligned_size_material,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		));
		
		uniform_buffers.sky.push_back(VulkanMemory::Buffer(aligned_size_sky,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		));
		
		// Storage Buffers
		storage_buffers.cull_commands.push_back(VulkanMemory::Buffer(total_mesh_count * aligned_size_cull_command,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		));

		storage_buffers.cull_stats.push_back(VulkanMemory::Buffer(aligned_size_cull_stats,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT  | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		));
		
		storage_buffers.cull_model.push_back(VulkanMemory::Buffer(aligned_size_cull_model,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, ////////////////////////////////////////////////////////////////////////////// VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		));

		scene.asset_manager.storage_buffer_bones.push_back(VulkanMemory::Buffer(total_bone_count * sizeof(Matrix4),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		));
	}

	// Allocate and update Descriptor Sets
	for (auto & texture : scene.asset_manager.textures) {
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
		std::vector<VkDescriptorSetLayout> layouts(swapchain_image_count, descriptor_set_layouts.cull);

		VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		alloc_info.descriptorPool = descriptor_pool;
		alloc_info.descriptorSetCount = layouts.size();
		alloc_info.pSetLayouts        = layouts.data();

		descriptor_sets.cull.resize(swapchain_image_count);
		VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, descriptor_sets.cull.data()));

		for (int i = 0; i < descriptor_sets.cull.size(); i++) {
			auto descriptor_set = descriptor_sets.cull[i];
			
			VkWriteDescriptorSet write_descriptors[4] = { };
			
			VkDescriptorBufferInfo descriptor_commands = { };
			descriptor_commands.buffer = storage_buffers.cull_commands[i].buffer;
			descriptor_commands.offset = 0;
			descriptor_commands.range = sizeof(IndexedIndirectCommand);

			write_descriptors[0].sType = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			write_descriptors[0].dstSet = descriptor_set;
			write_descriptors[0].dstBinding = 0;
			write_descriptors[0].dstArrayElement = 0;
			write_descriptors[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			write_descriptors[0].descriptorCount = 1;
			write_descriptors[0].pBufferInfo     = &descriptor_commands;
			
			VkDescriptorBufferInfo descriptor_camera = { };
			descriptor_camera.buffer = uniform_buffers.camera[i].buffer;
			descriptor_camera.offset = 0;
			descriptor_camera.range = sizeof(CameraUBO);

			write_descriptors[1].sType = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			write_descriptors[1].dstSet = descriptor_set;
			write_descriptors[1].dstBinding = 1;
			write_descriptors[1].dstArrayElement = 0;
			write_descriptors[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			write_descriptors[1].descriptorCount = 1;
			write_descriptors[1].pBufferInfo     = &descriptor_camera;
			
			VkDescriptorBufferInfo descriptor_stats = { };
			descriptor_stats.buffer = storage_buffers.cull_stats[i].buffer;
			descriptor_stats.offset = 0;
			descriptor_stats.range = sizeof(CameraUBO);

			write_descriptors[2].sType = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			write_descriptors[2].dstSet = descriptor_set;
			write_descriptors[2].dstBinding = 2;
			write_descriptors[2].dstArrayElement = 0;
			write_descriptors[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			write_descriptors[2].descriptorCount = 1;
			write_descriptors[2].pBufferInfo     = &descriptor_stats;
			
			VkDescriptorBufferInfo descriptor_model = { };
			descriptor_model.buffer = storage_buffers.cull_model[i].buffer;
			descriptor_model.offset = 0;
			descriptor_model.range = sizeof(CameraUBO);

			write_descriptors[3].sType = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			write_descriptors[3].dstSet = descriptor_set;
			write_descriptors[3].dstBinding = 3;
			write_descriptors[3].dstArrayElement = 0;
			write_descriptors[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			write_descriptors[3].descriptorCount = 1;
			write_descriptors[3].pBufferInfo     = &descriptor_model;

			vkUpdateDescriptorSets(device, Util::array_element_count(write_descriptors), write_descriptors, 0, nullptr);
		}
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
	
	vkDestroyDescriptorSetLayout(device, descriptor_set_layouts.cull,     nullptr);
	vkDestroyDescriptorSetLayout(device, descriptor_set_layouts.geometry, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptor_set_layouts.material, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptor_set_layouts.bones,    nullptr);
	vkDestroyDescriptorSetLayout(device, descriptor_set_layouts.sky,      nullptr);
	
	vkDestroyPipelineLayout(device, pipeline_layouts.cull,              nullptr);
	vkDestroyPipelineLayout(device, pipeline_layouts.geometry_static,   nullptr);
	vkDestroyPipelineLayout(device, pipeline_layouts.geometry_animated, nullptr);
	vkDestroyPipelineLayout(device, pipeline_layouts.sky,               nullptr);

	vkDestroyPipeline(device, pipelines.cull,              nullptr);
	vkDestroyPipeline(device, pipelines.geometry_static,   nullptr);
	vkDestroyPipeline(device, pipelines.geometry_animated, nullptr);
	vkDestroyPipeline(device, pipelines.sky,               nullptr);
	
	uniform_buffers.material.clear();
	uniform_buffers.sky     .clear();

	vkDestroyRenderPass(device, render_pass, nullptr);
}

void RenderTaskGBuffer::render(int image_index, VkCommandBuffer command_buffer) {
	VkRenderPassBeginInfo render_pass_begin_info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	render_pass_begin_info.renderPass =  render_pass;
	render_pass_begin_info.framebuffer = render_target.frame_buffer;
	render_pass_begin_info.renderArea.extent.width  = width;
	render_pass_begin_info.renderArea.extent.height = height;
	render_pass_begin_info.clearValueCount = render_target.clear_values.size();
	render_pass_begin_info.pClearValues    = render_target.clear_values.data();

	vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
	
	auto last_texture_handle = -1;

	auto & uniform_buffer_material = uniform_buffers.material[image_index];
	auto & descriptor_set_material = descriptor_sets.material[image_index];
	
	auto aligned_size = Math::round_up(sizeof(MaterialUBO), VulkanContext::get_min_uniform_buffer_alignment());
	auto total_mesh_count = scene.animated_meshes.size() + scene.meshes.size();

	std::vector<std::byte> buffer_material_ubo(total_mesh_count * aligned_size);
	std::vector<std::byte> buffer_bones;

	auto num_unculled_mesh_instances = 0;
	auto bone_offset = 0;

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.geometry_animated);
	
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layouts.geometry_animated, 2, 1, &descriptor_sets.bones[image_index], 0, nullptr);

	for (int i = 0; i < scene.animated_meshes.size(); i++) {
		auto const & mesh_instance = scene.animated_meshes[i];
		auto const & mesh          = scene.asset_manager.get_animated_mesh(mesh_instance.mesh_handle);
		
		GBufferPushConstants push_constants = { };
		push_constants.world           = mesh_instance.transform.matrix;
		push_constants.view_projection = scene.camera.get_view_projection();		
		push_constants.bone_offset = bone_offset;

		vkCmdPushConstants(command_buffer, pipeline_layouts.geometry_animated, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GBufferPushConstants), &push_constants);
			
		auto ubo = reinterpret_cast<MaterialUBO *>(&buffer_material_ubo[num_unculled_mesh_instances * aligned_size]);
		ubo->material_roughness = mesh_instance.material->roughness;
		ubo->material_metallic  = mesh_instance.material->metallic;

		u32 offset = num_unculled_mesh_instances * aligned_size;
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layouts.geometry_animated, 1, 1, &descriptor_set_material, 1, &offset);
		num_unculled_mesh_instances++;
			
		VkBuffer     vertex_buffers[] = { mesh.vertex_buffer.buffer };
		VkDeviceSize vertex_offsets[] = { 0 };
		vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, vertex_offsets);

		vkCmdBindIndexBuffer(command_buffer, mesh.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		
		buffer_bones.resize(buffer_bones.size() + mesh.bones.size() * sizeof(Matrix4));
		std::memcpy(buffer_bones.data() + bone_offset * sizeof(Matrix4), mesh_instance.bone_transforms.data(), mesh_instance.bone_transforms.size() * sizeof(Matrix4));

		for (int j = 0; j < mesh.sub_meshes.size(); j++) {
			auto const & sub_mesh = mesh.sub_meshes[j];
			
			if (last_texture_handle != sub_mesh.texture_handle) {
				last_texture_handle  = sub_mesh.texture_handle;

				auto const & texture = scene.asset_manager.textures[sub_mesh.texture_handle];
				vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layouts.geometry_animated, 0, 1, &texture.descriptor_set, 0, nullptr);
			}

			vkCmdDrawIndexed(command_buffer, sub_mesh.index_count, 1, sub_mesh.index_offset, 0, 0);
		}
		
		bone_offset += mesh.bones.size();
	}
	
	if (buffer_bones.size() > 0) VulkanMemory::buffer_copy_direct(scene.asset_manager.storage_buffer_bones[image_index], buffer_bones.data(), buffer_bones.size());

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.geometry_static);
	
	for (int i = 0; i < scene.meshes.size(); i++) {
		auto const & mesh_instance = scene.meshes[i];
		auto const & mesh          = scene.asset_manager.get_mesh(mesh_instance.mesh_handle);

		auto     transform = mesh_instance.transform.matrix;
		auto abs_transform = Matrix4::abs(transform);

		auto first_sub_mesh = true;

		for (int j = 0; j < mesh.sub_meshes.size(); j++) {
			auto const & sub_mesh = mesh.sub_meshes[j];

			// Transform AABB into world space for culling
			auto center = 0.5f * (sub_mesh.aabb.min + sub_mesh.aabb.max);
			auto extent = 0.5f * (sub_mesh.aabb.max - sub_mesh.aabb.min);

			auto new_center = Matrix4::transform_position (    transform, center);
			auto new_extent = Matrix4::transform_direction(abs_transform, extent);

			auto aabb_world_min = new_center - new_extent;
			auto aabb_world_max = new_center + new_extent;

			if (scene.camera.frustum.intersect_aabb(aabb_world_min, aabb_world_max) == Frustum::IntersectionType::FULLY_OUTSIDE) continue;

			// The first unculled Submesh must set the Push Constants, bind Descriptor Sets, and bind Vertex/Index Buffers
			if (first_sub_mesh) {		
				first_sub_mesh = false;

				GBufferPushConstants push_constants = { };
				push_constants.world = transform;
				push_constants.wvp   = scene.camera.get_view_projection() * transform;

				vkCmdPushConstants(command_buffer, pipeline_layouts.geometry_static, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GBufferPushConstants), &push_constants);
				
				auto ubo = reinterpret_cast<MaterialUBO *>(&buffer_material_ubo[num_unculled_mesh_instances * aligned_size]);
				ubo->material_roughness = mesh_instance.material->roughness;
				ubo->material_metallic  = mesh_instance.material->metallic;

				u32 offset = num_unculled_mesh_instances * aligned_size;
				vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layouts.geometry_static, 1, 1, &descriptor_set_material, 1, &offset);
				num_unculled_mesh_instances++;
				
				VkBuffer     vertex_buffers[] = { mesh.vertex_buffer.buffer };
				VkDeviceSize vertex_offsets[] = { 0 };
				vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, vertex_offsets);

				vkCmdBindIndexBuffer(command_buffer, mesh.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
			}

			if (last_texture_handle != sub_mesh.texture_handle) {
				last_texture_handle  = sub_mesh.texture_handle;

				auto const & texture = scene.asset_manager.textures[sub_mesh.texture_handle];
				vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layouts.geometry_static, 0, 1, &texture.descriptor_set, 0, nullptr);
			}
			
			vkCmdDrawIndexed(command_buffer, sub_mesh.index_count, 1, sub_mesh.index_offset, 0, 0);
		}
	}

	if (num_unculled_mesh_instances > 0) VulkanMemory::buffer_copy_direct(uniform_buffer_material, buffer_material_ubo.data(), num_unculled_mesh_instances * aligned_size);
	
	// Render Sky
	auto const & uniform_buffer_sky = uniform_buffers.sky[image_index];
	auto const & descriptor_set_sky = descriptor_sets.sky[image_index];

	vkCmdBindPipeline      (command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.sky);
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layouts.sky, 0, 1, &descriptor_set_sky, 0, nullptr);

	SkyUBO sky_ubo = { };
	sky_ubo.camera_top_left_corner = scene.camera.get_top_left_corner();
	sky_ubo.camera_x               = scene.camera.get_x_axis();
	sky_ubo.camera_y               = scene.camera.get_y_axis();
	sky_ubo.sun_direction = -scene.directional_lights[0].get_direction();

	VulkanMemory::buffer_copy_direct(uniform_buffer_sky, &sky_ubo, sizeof(sky_ubo));

	vkCmdDraw(command_buffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(command_buffer);
}
