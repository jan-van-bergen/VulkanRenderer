#include "Renderer.h"

#include <Imgui/imgui.h>
#include <Imgui/imgui_impl_glfw.h>
#include <Imgui/imgui_impl_vulkan.h>

#include "VulkanCheck.h"
#include "VulkanContext.h"

#include "Vector2.h"
#include "Vector3.h"
#include "Matrix4.h"

#include "Input.h"
#include "Util.h"

Renderer::Renderer(GLFWwindow * window, u32 width, u32 height) : 
	scene(width, height),
	render_task_gbuffer     (scene),
	render_task_shadow      (scene),
	render_task_lighting    (scene),
	render_task_post_process(scene)
{
	auto device = VulkanContext::get_device();

	this->width  = width;
	this->height = height;

	this->window = window;

	PointLight::init_sphere();

	swapchain_create();

	VkSemaphoreCreateInfo semaphore_create_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

	VkFenceCreateInfo fence_create_info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		VK_CHECK(vkCreateSemaphore(device, &semaphore_create_info, nullptr, &semaphores_image_available[i]));
		VK_CHECK(vkCreateSemaphore(device, &semaphore_create_info, nullptr, &semaphores_render_done    [i]));

		VK_CHECK(vkCreateFence(device, &fence_create_info, nullptr, &fences[i]));
	}
}

Renderer::~Renderer() {
	auto device = VulkanContext::get_device();

	swapchain_destroy();

	PointLight::free_sphere();

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(device, semaphores_image_available[i], nullptr);
		vkDestroySemaphore(device, semaphores_render_done    [i], nullptr);

		vkDestroyFence(device, fences[i], nullptr);
	}
}

void Renderer::swapchain_create() {
	swapchain = VulkanContext::create_swapchain(width, height);

	auto device = VulkanContext::get_device();

	// Create Swapchain and its Image Views
	u32                  swapchain_image_count;                   vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, nullptr);
	std::vector<VkImage> swapchain_images(swapchain_image_count); vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, swapchain_images.data());

	swapchain_views .resize(swapchain_image_count);
	fences_in_flight.resize(swapchain_image_count, nullptr);

	for (int i = 0; i < swapchain_image_count; i++) {
		swapchain_views[i] = VulkanMemory::create_image_view(swapchain_images[i], 1, VulkanContext::FORMAT.format, VK_IMAGE_ASPECT_COLOR_BIT);
	}
	
	// Create Descriptor Pool
	VkDescriptorPoolSize descriptor_pool_sizes[] = {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1024 }
	};

	VkDescriptorPoolCreateInfo pool_create_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pool_create_info.poolSizeCount = Util::array_element_count(descriptor_pool_sizes);
	pool_create_info.pPoolSizes    = descriptor_pool_sizes;
	pool_create_info.maxSets =
		(1 + 1 + 3 + 1 + 2 * scene.asset_manager.meshes.size()) * swapchain_views.size() + // Sky + Materials + 3 Light types + Post Process + Bones
		scene.asset_manager.textures.size() +                                              // Textures
		scene.directional_lights.size();                                                   // Shadow map

	VK_CHECK(vkCreateDescriptorPool(device, &pool_create_info, nullptr, &descriptor_pool));

	render_task_gbuffer     .init(descriptor_pool, width, height, swapchain_views.size());
	render_task_shadow      .init(descriptor_pool, swapchain_views.size());
	render_task_lighting    .init(descriptor_pool, width, height, swapchain_views.size(), render_task_gbuffer .get_render_target());
	render_task_post_process.init(descriptor_pool, width, height, swapchain_views.size(), render_task_lighting.get_render_target(), window);

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
		frame_buffers[i] = VulkanContext::create_frame_buffer(width, height, render_task_post_process.get_render_pass(), { swapchain_views[i], depth_image_view });
	}

	// Create Command Buffers
	command_buffers.resize(swapchain_views.size());

	VkCommandBufferAllocateInfo command_buffer_alloc_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	command_buffer_alloc_info.commandPool = VulkanContext::get_command_pool();
	command_buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	command_buffer_alloc_info.commandBufferCount = command_buffers.size();

	VK_CHECK(vkAllocateCommandBuffers(device, &command_buffer_alloc_info, command_buffers.data()));
}

void Renderer::swapchain_destroy() {
	auto device       = VulkanContext::get_device();
	auto command_pool = VulkanContext::get_command_pool();
	
	render_task_gbuffer     .free();
	render_task_shadow      .free();
	render_task_lighting    .free();
	render_task_post_process.free();
	
	for (auto & storage_buffer : scene.asset_manager.storage_buffer_bones) {
		VulkanMemory::buffer_free(storage_buffer);
	}

	vkDestroyImage    (device, depth_image,        nullptr);
	vkDestroyImageView(device, depth_image_view,   nullptr);
	vkFreeMemory      (device, depth_image_memory, nullptr);
	
	vkDestroyDescriptorPool(device, descriptor_pool, nullptr);

	vkFreeCommandBuffers(device, command_pool, command_buffers.size(), command_buffers.data());
	
	for (int i = 0; i < swapchain_views.size(); i++) {
		vkDestroyFramebuffer(device, frame_buffers[i], nullptr);
		vkDestroyImageView  (device, swapchain_views[i], nullptr);
	}
	
	vkDestroySwapchainKHR(device, swapchain, nullptr);
}

void Renderer::update(float delta) {
	scene.update(delta);

	//int mouse_x, mouse_y; Input::get_mouse_pos(&mouse_x, &mouse_y);
	//if (render_task_post_process.gizmo_position.intersects_mouse(scene.camera, mouse_x, mouse_y)) {
	//	printf("test");
	//}

	// Update Timing
	timing.frame_delta = delta;

	constexpr int FRAME_HISTORY_LENGTH = 100;

	if (timing.frame_times.size() < 100) {
		timing.frame_times.push_back(delta);
	} else {
		timing.frame_times[timing.frame_index++ % FRAME_HISTORY_LENGTH] = delta;
	}
	
	timing.frame_avg = 0.0f;
	timing.frame_min = INFINITY;
	timing.frame_max = 0.0f;

	for (int i = 0; i < timing.frame_times.size(); i++) {
		timing.frame_avg += timing.frame_times[i];
		timing.frame_min = std::min(timing.frame_min, timing.frame_times[i]);
		timing.frame_max = std::max(timing.frame_max, timing.frame_times[i]);
	}
	timing.frame_avg /= float(timing.frame_times.size());

	timing.time_since_last_second += delta;
	timing.frames_since_last_second++;

	while (timing.time_since_last_second >= 1.0f) {
		timing.time_since_last_second -= 1.0f;

		timing.fps = timing.frames_since_last_second;
		timing.frames_since_last_second = 0;
	}
	
	// Update GUI
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Vulkan Renderer");
	ImGui::Text("Delta: %.2f ms", 1000.0f * timing.frame_delta);
	ImGui::Text("Avg:   %.2f ms", 1000.0f * timing.frame_avg);
	ImGui::Text("Min:   %.2f ms", 1000.0f * timing.frame_min);
	ImGui::Text("Max:   %.2f ms", 1000.0f * timing.frame_max);
	ImGui::Text("FPS:   %d", timing.fps);
	ImGui::End();

	static AnimatedMeshInstance * selected_animated_mesh = nullptr;
	static MeshInstance         * selected_mesh          = nullptr;

	ImGui::Begin("Scene");

	auto dir = scene.directional_lights[0].get_direction();
	ImGui::Text("Sun: %f, %f, %f", dir.x, dir.y, dir.z);
	ImGui::Text("Culled Lights %i/%i", render_task_lighting.num_culled_lights, scene.point_lights.size() + scene.spot_lights.size());

	if (ImGui::Button("Animation")) {
		auto & anim_mesh = scene.animated_meshes[2];

		if (anim_mesh.is_playing()) {
			anim_mesh.stop_animation();
		} else {
			anim_mesh.play_animation("Armature|Bend");
		}
	}

	for (auto & mesh : scene.animated_meshes) {
		if (ImGui::Button(mesh.name.c_str())) {
			selected_mesh = nullptr;
			selected_animated_mesh = & mesh;
		}
	}

	for (auto & mesh : scene.meshes) {
		if (ImGui::Button(mesh.name.c_str())) {
			selected_mesh = &mesh;
			selected_animated_mesh = nullptr;
		}
	}
	
	if (selected_animated_mesh) {
		ImGui::Text("Selected Animated Mesh: %s", selected_animated_mesh->name.c_str());
		ImGui::SliderFloat3("Position", selected_animated_mesh->transform.position.data, -10.0f, 10.0f);
		ImGui::SliderFloat4("Rotation", selected_animated_mesh->transform.rotation.data,  -1.0f, -1.0f);
		ImGui::SliderFloat ("Scale",   &selected_animated_mesh->transform.scale, 0.0f, 10.0f);

		ImGui::Text("Material:");
		ImGui::SliderFloat("Roughness", &selected_animated_mesh->material->roughness, 0.0f, 1.0f);
		ImGui::SliderFloat("Metallic",  &selected_animated_mesh->material->metallic,  0.0f, 1.0f);

		ImGui::Text("Animation:");
	} else if (selected_mesh) {
		ImGui::Text("Selected Mesh: %s", selected_mesh->name.c_str());
		ImGui::SliderFloat3("Position", selected_mesh->transform.position.data, -10.0f, 10.0f);
		ImGui::SliderFloat4("Rotation", selected_mesh->transform.rotation.data,  -1.0f, -1.0f);
		ImGui::SliderFloat ("Scale",   &selected_mesh->transform.scale, 0.0f, 10.0f);

		ImGui::Text("Material:");
		ImGui::SliderFloat("Roughness", &selected_mesh->material->roughness, 0.0f, 1.0f);
		ImGui::SliderFloat("Metallic",  &selected_mesh->material->metallic,  0.0f, 1.0f);
	}

	ImGui::End();
	ImGui::Render();
}

void Renderer::render() {
	auto device         = VulkanContext::get_device();
	auto queue_graphics = VulkanContext::get_queue_graphics();
	auto queue_present  = VulkanContext::get_queue_present();

	auto semaphore_image_available = semaphores_image_available[current_frame];
	auto semaphore_render_done     = semaphores_render_done    [current_frame];

	auto fence = fences[current_frame];

	// Wait until previous Frame is done
	VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));
	
	u32 image_index; VK_CHECK(vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, semaphore_image_available, VK_NULL_HANDLE, &image_index));

	// Recrod Command buffer
	auto & command_buffer = command_buffers[image_index];
	auto & frame_buffer   = frame_buffers  [image_index];

	VkCommandBufferBeginInfo command_buffer_begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	VK_CHECK(vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info));

	render_task_gbuffer     .render(image_index, command_buffer);
	render_task_shadow      .render(image_index, command_buffer);
	render_task_lighting    .render(image_index, command_buffer);
	render_task_post_process.render(image_index, command_buffer, frame_buffer);

	VK_CHECK(vkEndCommandBuffer(command_buffer));

	// Wait until ready to submit
	if (fences_in_flight[image_index] != nullptr) {
		VK_CHECK(vkWaitForFences(device, 1, &fences_in_flight[image_index], VK_TRUE, UINT64_MAX));
	}
	fences_in_flight[image_index] = fence;

	// Submit Command buffer
	VkSemaphore          wait_semaphores[] = { semaphore_image_available };
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

	// Present Swapchain
	VkSwapchainKHR swapchains[] = { swapchain };

	VkPresentInfoKHR present_info =  { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores    = &semaphore_render_done;
	present_info.swapchainCount = Util::array_element_count(swapchains);
	present_info.pSwapchains    = swapchains;
	present_info.pImageIndices  = &image_index;
	present_info.pResults = nullptr;

	auto result = vkQueuePresentKHR(queue_present, &present_info);
	
	// Check if we need to resize the Swapchain
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
