#include "RenderTaskPostProcess.h"

#include <Imgui/imgui.h>
#include <Imgui/imgui_impl_glfw.h>
#include <Imgui/imgui_impl_vulkan.h>

#include "VulkanCheck.h"
#include "VulkanContext.h"

#include "Util.h"

struct GizmoPushConstants {
	alignas(16) Matrix4 wvp;
	alignas(16) Vector3 colour;
};

RenderTaskPostProcess::RenderTaskPostProcess(Scene & scene) :
	scene(scene),
	gizmo_position(std::move(Gizmo::generate_position())),
	gizmo_rotation(std::move(Gizmo::generate_rotation()))
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
}

RenderTaskPostProcess::~RenderTaskPostProcess() {
	ImGui::DestroyContext();
}

void RenderTaskPostProcess::init(VkDescriptorPool descriptor_pool, int width, int height, int swapchain_image_count, RenderTarget const & render_target_input, GLFWwindow * window) {
	auto device = VulkanContext::get_device();

	this->width  = width;
	this->height = height;

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

	VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_create_info, nullptr, &descriptor_set_layout));

	// Create Render Pass
	std::vector<VkAttachmentDescription> attachments(2);

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

	render_pass = VulkanContext::create_render_pass(attachments);

	// Create Pipeline Layouts
	VulkanContext::PipelineLayoutDetails pipeline_layout_details = { };
	pipeline_layout_details.descriptor_set_layouts = { descriptor_set_layout };

	pipeline_layouts.tonemap = VulkanContext::create_pipeline_layout(pipeline_layout_details);

	VkPushConstantRange push_constants;
	push_constants.offset = 0;
	push_constants.size = sizeof(GizmoPushConstants);
	push_constants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	pipeline_layout_details.descriptor_set_layouts = { };
	pipeline_layout_details.push_constants = { push_constants };

	pipeline_layouts.gizmo = VulkanContext::create_pipeline_layout(pipeline_layout_details);

	// Create Pipelines
	VulkanContext::PipelineDetails pipeline_details;
	pipeline_details.width  = width;
	pipeline_details.height = height;
	pipeline_details.cull_mode = VK_CULL_MODE_NONE;
	pipeline_details.blends = { VulkanContext::PipelineDetails::BLEND_NONE };
	pipeline_details.shaders = {
		{ "Shaders/post_process.vert.spv", VK_SHADER_STAGE_VERTEX_BIT },
		{ "Shaders/post_process.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT }
	};
	pipeline_details.enable_depth_test  = false;
	pipeline_details.enable_depth_write = false;
	pipeline_details.pipeline_layout = pipeline_layouts.tonemap;
	pipeline_details.render_pass     = render_pass;

	pipelines.tonemap = VulkanContext::create_pipeline(pipeline_details);

	std::vector<VkVertexInputBindingDescription> gizmo_vertex_bindings(1);
	gizmo_vertex_bindings[0].binding = 0;
	gizmo_vertex_bindings[0].stride = sizeof(Vector3);
	gizmo_vertex_bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	std::vector<VkVertexInputAttributeDescription> gizmo_vertex_attributes(1);
	gizmo_vertex_attributes[0].binding  = 0;
	gizmo_vertex_attributes[0].location = 0;
	gizmo_vertex_attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	gizmo_vertex_attributes[0].offset = 0;

	pipeline_details.vertex_bindings   = Gizmo::Vertex::get_binding_descriptions();
	pipeline_details.vertex_attributes = Gizmo::Vertex::get_attribute_descriptions();
	pipeline_details.cull_mode = VK_CULL_MODE_BACK_BIT;
	pipeline_details.shaders = {
		{ "Shaders/gizmo.vert.spv", VK_SHADER_STAGE_VERTEX_BIT },
		{ "Shaders/gizmo.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT }
	};
	pipeline_details.enable_depth_test  = true;
	pipeline_details.enable_depth_write = true;
	pipeline_details.pipeline_layout = pipeline_layouts.gizmo;

	pipelines.gizmo = VulkanContext::create_pipeline(pipeline_details);

	// Allocate and update Descriptor Sets
	std::vector<VkDescriptorSetLayout> layouts(swapchain_image_count, descriptor_set_layout);

	VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = layouts.size();
	alloc_info.pSetLayouts        = layouts.data();

	descriptor_sets.resize(swapchain_image_count);
	VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, descriptor_sets.data()));

	for (int i = 0; i < descriptor_sets.size(); i++) {
		auto & descriptor_set = descriptor_sets[i];

		VkWriteDescriptorSet write_descriptor_sets[1] = { };

		VkDescriptorImageInfo descriptor_image_colour = { };
		descriptor_image_colour.sampler     = render_target_input.sampler;
		descriptor_image_colour.imageView   = render_target_input.attachments[0].image_view;
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

	// Init GUI
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
	pool_create_info.maxSets = swapchain_image_count;

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
	init_info.ImageCount    = swapchain_image_count;
	init_info.CheckVkResultFn = [](VkResult result) { VK_CHECK(result); };

	ImGui_ImplVulkan_Init(&init_info, render_pass);

	VkCommandBuffer command_buffer = VulkanMemory::command_buffer_single_use_begin();
	ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

	VulkanMemory::command_buffer_single_use_end(command_buffer);
}

void RenderTaskPostProcess::free() {
	auto device = VulkanContext::get_device();

	vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);

	vkDestroyPipelineLayout(device, pipeline_layouts.tonemap, nullptr);
	vkDestroyPipelineLayout(device, pipeline_layouts.gizmo,   nullptr);

	vkDestroyPipeline(device, pipelines.tonemap, nullptr);
	vkDestroyPipeline(device, pipelines.gizmo,   nullptr);

	vkDestroyRenderPass(device, render_pass, nullptr);

	vkDestroyDescriptorPool(device, descriptor_pool_gui, nullptr);

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
}

void RenderTaskPostProcess::render(int image_index, VkCommandBuffer command_buffer, VkFramebuffer frame_buffer) {
	VkClearValue clear[2] = { };
	clear[0].color        = { 0.0f, 0.0f, 0.0f, 0.0f };
	clear[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderpass_begin_info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	renderpass_begin_info.renderPass  = render_pass;
	renderpass_begin_info.framebuffer = frame_buffer;
	renderpass_begin_info.renderArea = { 0, 0, u32(width), u32(height) };
	renderpass_begin_info.clearValueCount = Util::array_element_count(clear);
	renderpass_begin_info.pClearValues    = clear;

	vkCmdBeginRenderPass(command_buffer, &renderpass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

	// Render tonemapped image
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.tonemap);

	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layouts.tonemap, 0, 1, &descriptor_sets[image_index], 0, nullptr);
	vkCmdDraw(command_buffer, 3, 1, 0, 0);

	// Render Gizmos
	//vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.gizmo);
	//
	//static Quaternion axis_rotations[3] = {
	//	Quaternion::axis_angle(Vector3(0.0f, 1.0f, 0.0f), DEG_TO_RAD(-90)),
	//	Quaternion::axis_angle(Vector3(1.0f, 0.0f, 0.0f), DEG_TO_RAD(+90)),
	//	Quaternion::identity()
	//};
	//static Vector3 axis_colours[3] = { Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f) };

	//Gizmo gizmos[] = { gizmo_position, gizmo_rotation };

	//for (auto const & gizmo : gizmos) {
	//	VkBuffer     vertex_buffers[] = { gizmo.vertex_buffer.buffer };
	//	VkDeviceSize vertex_offsets[] = { 0 };
	//	vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, vertex_offsets);

	//	vkCmdBindIndexBuffer(command_buffer, gizmo.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

	//	for (int axis = 0; axis < 3; axis++) {
	//		GizmoPushConstants push_constants = { };
	//		push_constants.wvp    = scene.camera.get_view_projection() * Matrix4::create_rotation(axis_rotations[axis]);
	//		push_constants.colour = axis_colours[axis];

	//		vkCmdPushConstants(command_buffer, pipeline_layouts.gizmo, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GizmoPushConstants), &push_constants);
	//
	//		vkCmdDrawIndexed(command_buffer, gizmo.indices.size(), 1, 0, 0, 0);
	//	}
	//}

	// Render GUI
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);

	vkCmdEndRenderPass(command_buffer);
}
