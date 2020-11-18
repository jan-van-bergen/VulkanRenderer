#include "RenderTaskLighting.h"

#include "VulkanCheck.h"
#include "VulkanContext.h"

#include "Matrix4.h"

#include "Util.h"

namespace IcoSphere {
	static Vector3 vertices[] = { Vector3(0.000000f, -1.000000f, 0.000000f), Vector3(0.425323f, -0.850654f, 0.309011f), Vector3(-0.162456f, -0.850654f, 0.499995f), Vector3(0.723607f, -0.447220f, 0.525725f), Vector3(0.425323f, -0.850654f, 0.309011f), Vector3(0.850648f, -0.525736f, 0.000000f), Vector3(0.000000f, -1.000000f, 0.000000f), Vector3(-0.162456f, -0.850654f, 0.499995f), Vector3(-0.525730f, -0.850652f, 0.000000f), Vector3(0.000000f, -1.000000f, 0.000000f), Vector3(-0.525730f, -0.850652f, 0.000000f), Vector3(-0.162456f, -0.850654f, -0.499995f), Vector3(0.000000f, -1.000000f, 0.000000f), Vector3(-0.162456f, -0.850654f, -0.499995f), Vector3(0.425323f, -0.850654f, -0.309011f), Vector3(0.723607f, -0.447220f, 0.525725f), Vector3(0.850648f, -0.525736f, 0.000000f), Vector3(0.951058f, 0.000000f, 0.309013f), Vector3(-0.276388f, -0.447220f, 0.850649f), Vector3(0.262869f, -0.525738f, 0.809012f), Vector3(0.000000f, 0.000000f, 1.000000f), Vector3(-0.894426f, -0.447216f, 0.000000f), Vector3(-0.688189f, -0.525736f, 0.499997f), Vector3(-0.951058f, 0.000000f, 0.309013f), Vector3(-0.276388f, -0.447220f, -0.850649f), Vector3(-0.688189f, -0.525736f, -0.499997f), Vector3(-0.587786f, 0.000000f, -0.809017f), Vector3(0.723607f, -0.447220f, -0.525725f), Vector3(0.262869f, -0.525738f, -0.809012f), Vector3(0.587786f, 0.000000f, -0.809017f), Vector3(0.723607f, -0.447220f, 0.525725f), Vector3(0.951058f, 0.000000f, 0.309013f), Vector3(0.587786f, 0.000000f, 0.809017f), Vector3(-0.276388f, -0.447220f, 0.850649f), Vector3(0.000000f, 0.000000f, 1.000000f), Vector3(-0.587786f, 0.000000f, 0.809017f), Vector3(-0.894426f, -0.447216f, 0.000000f), Vector3(-0.951058f, 0.000000f, 0.309013f), Vector3(-0.951058f, 0.000000f, -0.309013f), Vector3(-0.276388f, -0.447220f, -0.850649f), Vector3(-0.587786f, 0.000000f, -0.809017f), Vector3(0.000000f, 0.000000f, -1.000000f), Vector3(0.723607f, -0.447220f, -0.525725f), Vector3(0.587786f, 0.000000f, -0.809017f), Vector3(0.951058f, 0.000000f, -0.309013f), Vector3(0.276388f, 0.447220f, 0.850649f), Vector3(0.688189f, 0.525736f, 0.499997f), Vector3(0.162456f, 0.850654f, 0.499995f), Vector3(-0.723607f, 0.447220f, 0.525725f), Vector3(-0.262869f, 0.525738f, 0.809012f), Vector3(-0.425323f, 0.850654f, 0.309011f), Vector3(-0.723607f, 0.447220f, -0.525725f), Vector3(-0.850648f, 0.525736f, 0.000000f), Vector3(-0.425323f, 0.850654f, -0.309011f), Vector3(0.276388f, 0.447220f, -0.850649f), Vector3(-0.262869f, 0.525738f, -0.809012f), Vector3(0.162456f, 0.850654f, -0.499995f), Vector3(0.894426f, 0.447216f, 0.000000f), Vector3(0.688189f, 0.525736f, -0.499997f), Vector3(0.525730f, 0.850652f, 0.000000f), Vector3(0.525730f, 0.850652f, 0.000000f), Vector3(0.162456f, 0.850654f, -0.499995f), Vector3(0.000000f, 1.000000f, 0.000000f), Vector3(0.525730f, 0.850652f, 0.000000f), Vector3(0.688189f, 0.525736f, -0.499997f), Vector3(0.162456f, 0.850654f, -0.499995f), Vector3(0.688189f, 0.525736f, -0.499997f), Vector3(0.276388f, 0.447220f, -0.850649f), Vector3(0.162456f, 0.850654f, -0.499995f), Vector3(0.162456f, 0.850654f, -0.499995f), Vector3(-0.425323f, 0.850654f, -0.309011f), Vector3(0.000000f, 1.000000f, 0.000000f), Vector3(0.162456f, 0.850654f, -0.499995f), Vector3(-0.262869f, 0.525738f, -0.809012f), Vector3(-0.425323f, 0.850654f, -0.309011f), Vector3(-0.262869f, 0.525738f, -0.809012f), Vector3(-0.723607f, 0.447220f, -0.525725f), Vector3(-0.425323f, 0.850654f, -0.309011f), Vector3(-0.425323f, 0.850654f, -0.309011f), Vector3(-0.425323f, 0.850654f, 0.309011f), Vector3(0.000000f, 1.000000f, 0.000000f), Vector3(-0.425323f, 0.850654f, -0.309011f), Vector3(-0.850648f, 0.525736f, 0.000000f), Vector3(-0.425323f, 0.850654f, 0.309011f), Vector3(-0.850648f, 0.525736f, 0.000000f), Vector3(-0.723607f, 0.447220f, 0.525725f), Vector3(-0.425323f, 0.850654f, 0.309011f), Vector3(-0.425323f, 0.850654f, 0.309011f), Vector3(0.162456f, 0.850654f, 0.499995f), Vector3(0.000000f, 1.000000f, 0.000000f), Vector3(-0.425323f, 0.850654f, 0.309011f), Vector3(-0.262869f, 0.525738f, 0.809012f), Vector3(0.162456f, 0.850654f, 0.499995f), Vector3(-0.262869f, 0.525738f, 0.809012f), Vector3(0.276388f, 0.447220f, 0.850649f), Vector3(0.162456f, 0.850654f, 0.499995f), Vector3(0.162456f, 0.850654f, 0.499995f), Vector3(0.525730f, 0.850652f, 0.000000f), Vector3(0.000000f, 1.000000f, 0.000000f), Vector3(0.162456f, 0.850654f, 0.499995f), Vector3(0.688189f, 0.525736f, 0.499997f), Vector3(0.525730f, 0.850652f, 0.000000f), Vector3(0.688189f, 0.525736f, 0.499997f), Vector3(0.894426f, 0.447216f, 0.000000f), Vector3(0.525730f, 0.850652f, 0.000000f), Vector3(0.951058f, 0.000000f, -0.309013f), Vector3(0.688189f, 0.525736f, -0.499997f), Vector3(0.894426f, 0.447216f, 0.000000f), Vector3(0.951058f, 0.000000f, -0.309013f), Vector3(0.587786f, 0.000000f, -0.809017f), Vector3(0.688189f, 0.525736f, -0.499997f), Vector3(0.587786f, 0.000000f, -0.809017f), Vector3(0.276388f, 0.447220f, -0.850649f), Vector3(0.688189f, 0.525736f, -0.499997f), Vector3(0.000000f, 0.000000f, -1.000000f), Vector3(-0.262869f, 0.525738f, -0.809012f), Vector3(0.276388f, 0.447220f, -0.850649f), Vector3(0.000000f, 0.000000f, -1.000000f), Vector3(-0.587786f, 0.000000f, -0.809017f), Vector3(-0.262869f, 0.525738f, -0.809012f), Vector3(-0.587786f, 0.000000f, -0.809017f), Vector3(-0.723607f, 0.447220f, -0.525725f), Vector3(-0.262869f, 0.525738f, -0.809012f), Vector3(-0.951058f, 0.000000f, -0.309013f), Vector3(-0.850648f, 0.525736f, 0.000000f), Vector3(-0.723607f, 0.447220f, -0.525725f), Vector3(-0.951058f, 0.000000f, -0.309013f), Vector3(-0.951058f, 0.000000f, 0.309013f), Vector3(-0.850648f, 0.525736f, 0.000000f), Vector3(-0.951058f, 0.000000f, 0.309013f), Vector3(-0.723607f, 0.447220f, 0.525725f), Vector3(-0.850648f, 0.525736f, 0.000000f), Vector3(-0.587786f, 0.000000f, 0.809017f), Vector3(-0.262869f, 0.525738f, 0.809012f), Vector3(-0.723607f, 0.447220f, 0.525725f), Vector3(-0.587786f, 0.000000f, 0.809017f), Vector3(0.000000f, 0.000000f, 1.000000f), Vector3(-0.262869f, 0.525738f, 0.809012f), Vector3(0.000000f, 0.000000f, 1.000000f), Vector3(0.276388f, 0.447220f, 0.850649f), Vector3(-0.262869f, 0.525738f, 0.809012f), Vector3(0.587786f, 0.000000f, 0.809017f), Vector3(0.688189f, 0.525736f, 0.499997f), Vector3(0.276388f, 0.447220f, 0.850649f), Vector3(0.587786f, 0.000000f, 0.809017f), Vector3(0.951058f, 0.000000f, 0.309013f), Vector3(0.688189f, 0.525736f, 0.499997f), Vector3(0.951058f, 0.000000f, 0.309013f), Vector3(0.894426f, 0.447216f, 0.000000f), Vector3(0.688189f, 0.525736f, 0.499997f), Vector3(0.587786f, 0.000000f, -0.809017f), Vector3(0.000000f, 0.000000f, -1.000000f), Vector3(0.276388f, 0.447220f, -0.850649f), Vector3(0.587786f, 0.000000f, -0.809017f), Vector3(0.262869f, -0.525738f, -0.809012f), Vector3(0.000000f, 0.000000f, -1.000000f), Vector3(0.262869f, -0.525738f, -0.809012f), Vector3(-0.276388f, -0.447220f, -0.850649f), Vector3(0.000000f, 0.000000f, -1.000000f), Vector3(-0.587786f, 0.000000f, -0.809017f), Vector3(-0.951058f, 0.000000f, -0.309013f), Vector3(-0.723607f, 0.447220f, -0.525725f), Vector3(-0.587786f, 0.000000f, -0.809017f), Vector3(-0.688189f, -0.525736f, -0.499997f), Vector3(-0.951058f, 0.000000f, -0.309013f), Vector3(-0.688189f, -0.525736f, -0.499997f), Vector3(-0.894426f, -0.447216f, 0.000000f), Vector3(-0.951058f, 0.000000f, -0.309013f), Vector3(-0.951058f, 0.000000f, 0.309013f), Vector3(-0.587786f, 0.000000f, 0.809017f), Vector3(-0.723607f, 0.447220f, 0.525725f), Vector3(-0.951058f, 0.000000f, 0.309013f), Vector3(-0.688189f, -0.525736f, 0.499997f), Vector3(-0.587786f, 0.000000f, 0.809017f), Vector3(-0.688189f, -0.525736f, 0.499997f), Vector3(-0.276388f, -0.447220f, 0.850649f), Vector3(-0.587786f, 0.000000f, 0.809017f), Vector3(0.000000f, 0.000000f, 1.000000f), Vector3(0.587786f, 0.000000f, 0.809017f), Vector3(0.276388f, 0.447220f, 0.850649f), Vector3(0.000000f, 0.000000f, 1.000000f), Vector3(0.262869f, -0.525738f, 0.809012f), Vector3(0.587786f, 0.000000f, 0.809017f), Vector3(0.262869f, -0.525738f, 0.809012f), Vector3(0.723607f, -0.447220f, 0.525725f), Vector3(0.587786f, 0.000000f, 0.809017f), Vector3(0.951058f, 0.000000f, 0.309013f), Vector3(0.951058f, 0.000000f, -0.309013f), Vector3(0.894426f, 0.447216f, 0.000000f), Vector3(0.951058f, 0.000000f, 0.309013f), Vector3(0.850648f, -0.525736f, 0.000000f), Vector3(0.951058f, 0.000000f, -0.309013f), Vector3(0.850648f, -0.525736f, 0.000000f), Vector3(0.723607f, -0.447220f, -0.525725f), Vector3(0.951058f, 0.000000f, -0.309013f), Vector3(0.425323f, -0.850654f, -0.309011f), Vector3(0.262869f, -0.525738f, -0.809012f), Vector3(0.723607f, -0.447220f, -0.525725f), Vector3(0.425323f, -0.850654f, -0.309011f), Vector3(-0.162456f, -0.850654f, -0.499995f), Vector3(0.262869f, -0.525738f, -0.809012f), Vector3(-0.162456f, -0.850654f, -0.499995f), Vector3(-0.276388f, -0.447220f, -0.850649f), Vector3(0.262869f, -0.525738f, -0.809012f), Vector3(-0.162456f, -0.850654f, -0.499995f), Vector3(-0.688189f, -0.525736f, -0.499997f), Vector3(-0.276388f, -0.447220f, -0.850649f), Vector3(-0.162456f, -0.850654f, -0.499995f), Vector3(-0.525730f, -0.850652f, 0.000000f), Vector3(-0.688189f, -0.525736f, -0.499997f), Vector3(-0.525730f, -0.850652f, 0.000000f), Vector3(-0.894426f, -0.447216f, 0.000000f), Vector3(-0.688189f, -0.525736f, -0.499997f), Vector3(-0.525730f, -0.850652f, 0.000000f), Vector3(-0.688189f, -0.525736f, 0.499997f), Vector3(-0.894426f, -0.447216f, 0.000000f), Vector3(-0.525730f, -0.850652f, 0.000000f), Vector3(-0.162456f, -0.850654f, 0.499995f), Vector3(-0.688189f, -0.525736f, 0.499997f), Vector3(-0.162456f, -0.850654f, 0.499995f), Vector3(-0.276388f, -0.447220f, 0.850649f), Vector3(-0.688189f, -0.525736f, 0.499997f), Vector3(0.850648f, -0.525736f, 0.000000f), Vector3(0.425323f, -0.850654f, -0.309011f), Vector3(0.723607f, -0.447220f, -0.525725f), Vector3(0.850648f, -0.525736f, 0.000000f), Vector3(0.425323f, -0.850654f, 0.309011f), Vector3(0.425323f, -0.850654f, -0.309011f), Vector3(0.425323f, -0.850654f, 0.309011f), Vector3(0.000000f, -1.000000f, 0.000000f), Vector3(0.425323f, -0.850654f, -0.309011f), Vector3(-0.162456f, -0.850654f, 0.499995f), Vector3(0.262869f, -0.525738f, 0.809012f), Vector3(-0.276388f, -0.447220f, 0.850649f), Vector3(-0.162456f, -0.850654f, 0.499995f), Vector3(0.425323f, -0.850654f, 0.309011f), Vector3(0.262869f, -0.525738f, 0.809012f), Vector3(0.425323f, -0.850654f, 0.309011f), Vector3(0.723607f, -0.447220f, 0.525725f), Vector3(0.262869f, -0.525738f, 0.809012f), };
	static int     indices [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,  };
}

struct DirectionalLightUBO {
	struct {
		alignas(16) Vector3 colour;

		alignas(16) Vector3 direction;

		alignas(16) Matrix4 light_matrix;
	} directional_light;
	
	alignas(16) Vector3 camera_position;

	alignas(16) Matrix4 inv_view_projection;
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
	
	alignas(16) Matrix4 inv_view_projection;
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
	
	alignas(16) Matrix4 inv_view_projection;
};

RenderTaskLighting::PointLightSphere::PointLightSphere() :
	vertex_buffer(sizeof(IcoSphere::vertices), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
	index_buffer (sizeof(IcoSphere::indices),  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
{
	VulkanMemory::buffer_copy_staged(vertex_buffer, IcoSphere::vertices, sizeof(IcoSphere::vertices));
	VulkanMemory::buffer_copy_staged(index_buffer,  IcoSphere::indices,  sizeof(IcoSphere::indices));

	index_count = Util::array_element_count(IcoSphere::indices);
}

void RenderTaskLighting::LightPass::free() {
	auto device = VulkanContext::get_device();

	vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
	vkDestroyPipeline      (device, pipeline,        nullptr);
	
	uniform_buffers.clear();
}

RenderTaskLighting::LightPass RenderTaskLighting::create_light_pass(
	VkDescriptorPool descriptor_pool,
	int width, int height, int swapchain_image_count,
	RenderTarget const & render_target_input,
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
	pipeline_layout_details.descriptor_set_layouts = { descriptor_set_layouts.light, descriptor_set_layouts.shadow };
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
	pipeline_details.cull_mode = VK_CULL_MODE_NONE;
	pipeline_details.blends = { VulkanContext::PipelineDetails::BLEND_ADDITIVE };
	pipeline_details.shaders = {
		{ filename_shader_vertex,   VK_SHADER_STAGE_VERTEX_BIT },
		{ filename_shader_fragment, VK_SHADER_STAGE_FRAGMENT_BIT }
	};
	pipeline_details.enable_depth_test  = false;
	pipeline_details.enable_depth_write = false;
	pipeline_details.pipeline_layout = light_pass.pipeline_layout;
	pipeline_details.render_pass     = render_pass;

	light_pass.pipeline = VulkanContext::create_pipeline(pipeline_details);

	// Create Uniform Buffers
	light_pass.uniform_buffers.reserve(swapchain_image_count);

	auto aligned_size = Math::round_up(sizeof(PointLightUBO), VulkanContext::get_min_uniform_buffer_alignment());
	
	for (int i = 0; i < swapchain_image_count; i++) {
		light_pass.uniform_buffers.push_back(VulkanMemory::Buffer(scene.point_lights.size() * aligned_size,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		));
	}

	// Allocate and update Descriptor Sets
	std::vector<VkDescriptorSetLayout> layouts(swapchain_image_count, descriptor_set_layouts.light);

	VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = layouts.size();
	alloc_info.pSetLayouts        = layouts.data();

	light_pass.descriptor_sets.resize(swapchain_image_count);
	VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, light_pass.descriptor_sets.data()));

	for (int i = 0; i < light_pass.descriptor_sets.size(); i++) {
		auto & descriptor_set = light_pass.descriptor_sets[i];

		VkWriteDescriptorSet write_descriptor_sets[4] = { };

		// Write Descriptor for Albedo target
		VkDescriptorImageInfo descriptor_image_albedo = { };
		descriptor_image_albedo.sampler     = render_target_input.sampler;
		descriptor_image_albedo.imageView   = render_target_input.attachments[0].image_view;
		descriptor_image_albedo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		write_descriptor_sets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_descriptor_sets[0].dstSet = descriptor_set;
		write_descriptor_sets[0].dstBinding = 0;
		write_descriptor_sets[0].dstArrayElement = 0;
		write_descriptor_sets[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write_descriptor_sets[0].descriptorCount = 1;
		write_descriptor_sets[0].pImageInfo = &descriptor_image_albedo;
		
		// Write Descriptor for Normal target
		VkDescriptorImageInfo descriptor_image_normal = { };
		descriptor_image_normal.sampler     = render_target_input.sampler;
		descriptor_image_normal.imageView   = render_target_input.attachments[1].image_view;
		descriptor_image_normal.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		write_descriptor_sets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_descriptor_sets[1].dstSet = descriptor_set;
		write_descriptor_sets[1].dstBinding = 1;
		write_descriptor_sets[1].dstArrayElement = 0;
		write_descriptor_sets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write_descriptor_sets[1].descriptorCount = 1;
		write_descriptor_sets[1].pImageInfo = &descriptor_image_normal;
		
		// Write Descriptor for Depth target
		VkDescriptorImageInfo descriptor_image_depth = { };
		descriptor_image_depth.sampler     = render_target_input.sampler;
		descriptor_image_depth.imageView   = render_target_input.attachments[2].image_view;
		descriptor_image_depth.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		write_descriptor_sets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_descriptor_sets[2].dstSet = descriptor_set;
		write_descriptor_sets[2].dstBinding = 2;
		write_descriptor_sets[2].dstArrayElement = 0;
		write_descriptor_sets[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write_descriptor_sets[2].descriptorCount = 1;
		write_descriptor_sets[2].pImageInfo = &descriptor_image_depth;

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

void RenderTaskLighting::init(VkDescriptorPool descriptor_pool, int width, int height, int swapchain_image_count, RenderTarget const & render_target_input) {
	auto device = VulkanContext::get_device();

	this->width  = width;
	this->height = height;

	{
		// Create Descriptor Set Layout
		VkDescriptorSetLayoutBinding layout_bindings[4] = { };

		// Albedo Sampler
		layout_bindings[0].binding = 0;
		layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		layout_bindings[0].descriptorCount = 1;
		layout_bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		layout_bindings[0].pImmutableSamplers = nullptr;

		// Normal Sampler
		layout_bindings[1].binding = 1;
		layout_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		layout_bindings[1].descriptorCount = 1;
		layout_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		layout_bindings[1].pImmutableSamplers = nullptr;

		// Depth Sampler
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

		VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_create_info, nullptr, &descriptor_set_layouts.light));
	}

	{
		// Create Descriptor Set Layout
		VkDescriptorSetLayoutBinding layout_bindings[1] = { };

		// Shadow Map Sampler
		layout_bindings[0].binding = 0;
		layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		layout_bindings[0].descriptorCount = 1;
		layout_bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		layout_bindings[0].pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo layout_create_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		layout_create_info.bindingCount = Util::array_element_count(layout_bindings);
		layout_create_info.pBindings    = layout_bindings;

		VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_create_info, nullptr, &descriptor_set_layouts.shadow));
	}
	
	render_target.add_attachment(width, height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VulkanContext::create_clear_value_colour()); // HDR lighting
	
	render_pass = VulkanContext::create_render_pass(render_target.get_attachment_descriptions());
	render_target.init(width, height, render_pass);

	light_pass_directional = create_light_pass(
		descriptor_pool,
		width, height,
		swapchain_image_count,
		render_target_input,
		{ },
		{ },
		"Shaders/light_directional.vert.spv",
		"Shaders/light_directional.frag.spv",
		0,
		sizeof(DirectionalLightUBO)
	);

	light_pass_point = create_light_pass(
		descriptor_pool,
		width, height,
		swapchain_image_count,
		render_target_input,
		{ { 0, sizeof(Vector3), VK_VERTEX_INPUT_RATE_VERTEX } },
		{ { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 } },
		"Shaders/light_point.vert.spv",
		"Shaders/light_point.frag.spv",
		sizeof(PointLightPushConstants),
		sizeof(PointLightUBO)
	);

	light_pass_spot = create_light_pass(
		descriptor_pool,
		width, height,
		swapchain_image_count,
		render_target_input,
		{ { 0, sizeof(Vector3), VK_VERTEX_INPUT_RATE_VERTEX } },
		{ { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 } },
		"Shaders/light_spot.vert.spv",
		"Shaders/light_spot.frag.spv",
		sizeof(PointLightPushConstants),
		sizeof(SpotLightUBO)
	);
	
	// Allocate and update Descriptor Sets
	for (auto & directional_light : scene.directional_lights) {	
		VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		alloc_info.descriptorPool = descriptor_pool;
		alloc_info.descriptorSetCount = 1;
		alloc_info.pSetLayouts        = &descriptor_set_layouts.shadow;

		VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, &directional_light.shadow_map.descriptor_set));

		VkDescriptorImageInfo image_info;
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		image_info.imageView = directional_light.shadow_map.render_target.attachments[0].image_view;
		image_info.sampler   = directional_light.shadow_map.render_target.sampler;

		VkWriteDescriptorSet write_descriptor_set = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write_descriptor_set.dstSet = directional_light.shadow_map.descriptor_set;
		write_descriptor_set.dstBinding = 0;
		write_descriptor_set.dstArrayElement = 0;
		write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write_descriptor_set.descriptorCount = 1;
		write_descriptor_set.pImageInfo      = &image_info;

		vkUpdateDescriptorSets(device, 1, &write_descriptor_set, 0, nullptr);
	}
}

void RenderTaskLighting::free() {
	auto device = VulkanContext::get_device();

	light_pass_directional.free();
	light_pass_point      .free();
	light_pass_spot       .free();

	vkDestroyDescriptorSetLayout(device, descriptor_set_layouts.light,  nullptr);
	vkDestroyDescriptorSetLayout(device, descriptor_set_layouts.shadow, nullptr);
	
	render_target.free();

	vkDestroyRenderPass(device, render_pass, nullptr);
}

void RenderTaskLighting::render(int image_index, VkCommandBuffer command_buffer) {
	// Begin Light Render Pass
	VkRenderPassBeginInfo renderpass_begin_info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	renderpass_begin_info.renderPass  = render_pass;
	renderpass_begin_info.framebuffer = render_target.frame_buffer;
	renderpass_begin_info.renderArea = { 0, 0, u32(width), u32(height) };
	renderpass_begin_info.clearValueCount = render_target.clear_values.size();
	renderpass_begin_info.pClearValues    = render_target.clear_values.data();
		
	vkCmdBeginRenderPass(command_buffer, &renderpass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

	// Render Directional Lights
	if (scene.directional_lights.size() > 0) {
		auto & uniform_buffer = light_pass_directional.uniform_buffers[image_index];
		auto & descriptor_set = light_pass_directional.descriptor_sets[image_index];

		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, light_pass_directional.pipeline);
		
		// Upload Uniform Buffer
		auto aligned_size = Math::round_up(sizeof(DirectionalLightUBO), VulkanContext::get_min_uniform_buffer_alignment());
		
		std::vector<std::byte> buf(scene.directional_lights.size() * aligned_size);

		// For each Directional Light render a full sqreen quad
		for (int i = 0; i < scene.directional_lights.size(); i++) {
			auto const & directional_light = scene.directional_lights[i];

			auto ubo = reinterpret_cast<DirectionalLightUBO *>(&buf[i * aligned_size]);
			ubo->directional_light.colour       = directional_light.colour;
			ubo->directional_light.direction    = directional_light.get_direction();
			ubo->directional_light.light_matrix = directional_light.get_light_matrix();
			ubo->camera_position     = scene.camera.position;
			ubo->inv_view_projection = scene.camera.get_inv_view_projection();

			u32 offset = i * aligned_size;
			vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, light_pass_directional.pipeline_layout, 0, 1, &descriptor_set,                              1, &offset);	
			vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, light_pass_directional.pipeline_layout, 1, 1, &directional_light.shadow_map.descriptor_set, 0, nullptr);

			vkCmdDraw(command_buffer, 3, 1, 0, 0);
		}
	
		VulkanMemory::buffer_copy_direct(uniform_buffer, buf.data(), buf.size());
	}

	num_culled_lights = 0;

	// Render Point Lights
	if (scene.point_lights.size() > 0) {
		auto & uniform_buffer = light_pass_point.uniform_buffers[image_index];
		auto & descriptor_set = light_pass_point.descriptor_sets[image_index];

		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, light_pass_point.pipeline);

		// Upload Uniform Buffer
		auto aligned_size = Math::round_up(sizeof(PointLightUBO), VulkanContext::get_min_uniform_buffer_alignment());

		std::vector<std::byte> buf(scene.point_lights.size() * aligned_size);

		// Bind Sphere to render Point Lights
		VkBuffer     vertex_buffers[] = { point_light_sphere.vertex_buffer.buffer };
		VkDeviceSize vertex_offsets[] = { 0 };
		vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, vertex_offsets);

		vkCmdBindIndexBuffer(command_buffer, point_light_sphere.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		
		int num_unculled_lights = 0;

		// For each Point Light render a sphere with the appropriate radius and position
		for (int i = 0; i < scene.point_lights.size(); i++) {
			auto const & point_light = scene.point_lights[i];

			if (scene.camera.frustum.intersect_sphere(point_light.position, point_light.radius) == Frustum::IntersectionType::FULLY_OUTSIDE) continue;
			
			// Upload UBO
			auto ubo = reinterpret_cast<PointLightUBO *>(&buf[num_unculled_lights * aligned_size]);
			ubo->point_light.colour   = point_light.colour;
			ubo->point_light.position = point_light.position;
			ubo->point_light.one_over_radius_squared = 1.0f / (point_light.radius * point_light.radius);
			ubo->camera_position     = scene.camera.position;
			ubo->inv_view_projection = scene.camera.get_inv_view_projection();

			// Draw Sphere
			PointLightPushConstants push_constants = { };
			push_constants.wvp = scene.camera.get_view_projection() * Matrix4::create_translation(point_light.position) * Matrix4::create_scale(point_light.radius);

			vkCmdPushConstants(command_buffer, light_pass_point.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PointLightPushConstants), &push_constants);

			u32 offset = num_unculled_lights * aligned_size;
			vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, light_pass_point.pipeline_layout, 0, 1, &descriptor_set, 1, &offset);

			vkCmdDrawIndexed(command_buffer, point_light_sphere.index_count, 1, 0, 0, 0);

			num_unculled_lights++;
		}
		
		if (num_unculled_lights > 0) VulkanMemory::buffer_copy_direct(uniform_buffer, buf.data(), num_unculled_lights * aligned_size);

		num_culled_lights += scene.point_lights.size() - num_unculled_lights;
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
		VkBuffer     vertex_buffers[] = { point_light_sphere.vertex_buffer.buffer };
		VkDeviceSize vertex_offsets[] = { 0 };
		vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, vertex_offsets);

		vkCmdBindIndexBuffer(command_buffer, point_light_sphere.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		
		int num_unculled_lights = 0;

		// For each Spot Light render a sphere with the appropriate radius and position
		for (int i = 0; i < scene.spot_lights.size(); i++) {
			auto const & spot_light = scene.spot_lights[i];
			
			if (scene.camera.frustum.intersect_sphere(spot_light.position, spot_light.radius) == Frustum::IntersectionType::FULLY_OUTSIDE) continue;
			
			auto ubo = reinterpret_cast<SpotLightUBO *>(&buf[num_unculled_lights * aligned_size]);
			ubo->spot_light.colour    = spot_light.colour;
			ubo->spot_light.position  = spot_light.position;
			ubo->spot_light.direction = spot_light.direction;
			ubo->spot_light.one_over_radius_squared = 1.0f / (spot_light.radius * spot_light.radius);
			ubo->spot_light.cutoff_inner = std::cos(0.5f * spot_light.cutoff_inner);
			ubo->spot_light.cutoff_outer = std::cos(0.5f * spot_light.cutoff_outer);
			ubo->camera_position     = scene.camera.position;
			ubo->inv_view_projection = scene.camera.get_inv_view_projection();

			PointLightPushConstants push_constants = { };
			push_constants.wvp = scene.camera.get_view_projection() * Matrix4::create_translation(spot_light.position) * Matrix4::create_scale(spot_light.radius);

			vkCmdPushConstants(command_buffer, light_pass_spot.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PointLightPushConstants), &push_constants);

			u32 offset = num_unculled_lights * aligned_size;
			vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, light_pass_spot.pipeline_layout, 0, 1, &descriptor_set, 1, &offset);

			vkCmdDrawIndexed(command_buffer, point_light_sphere.index_count, 1, 0, 0, 0);

			num_unculled_lights++;
		}	
		
		if (num_unculled_lights > 0) VulkanMemory::buffer_copy_direct(uniform_buffer, buf.data(), num_unculled_lights * aligned_size);
		
		num_culled_lights += scene.spot_lights.size() - num_unculled_lights;
	}

	vkCmdEndRenderPass(command_buffer);
}
