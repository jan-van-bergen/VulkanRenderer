#pragma once
#include <string>
#include <vector>
#include <optional>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

#include "Types.h"

namespace VulkanContext {
	inline constexpr VkSurfaceFormatKHR FORMAT = { VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	inline constexpr VkPresentModeKHR   PRESENT_MODE = VK_PRESENT_MODE_MAILBOX_KHR;
	
	void init(GLFWwindow * window);
	void destroy();

	VkSwapchainKHR create_swapchain(u32 width, u32 height);

	VkFormat get_supported_depth_format();

	struct Shader {
		VkShaderModule                  module;
		VkPipelineShaderStageCreateInfo stage_create_info;
	};

	Shader shader_load(std::string const & filename, VkShaderStageFlagBits stage);
	
	VkRenderPass create_render_pass(std::vector<VkAttachmentDescription> const & attachments);

	struct PipelineLayoutDetails {
		std::vector<VkDescriptorSetLayout> descriptor_set_layouts;

		std::vector<VkPushConstantRange> push_constants;
	};
	VkPipelineLayout create_pipeline_layout(PipelineLayoutDetails const & details);

	struct PipelineDetails {
		std::vector<VkVertexInputBindingDescription>   vertex_bindings;
		std::vector<VkVertexInputAttributeDescription> vertex_attributes;

		int width;
		int height;

		VkCullModeFlagBits cull_mode = VK_CULL_MODE_BACK_BIT;

		inline static VkPipelineColorBlendAttachmentState constexpr BLEND_NONE = {
			VK_FALSE,
			VK_BLEND_FACTOR_ONE,
			VK_BLEND_FACTOR_ZERO,
			VK_BLEND_OP_ADD,
			VK_BLEND_FACTOR_ONE,
			VK_BLEND_FACTOR_ZERO,
			VK_BLEND_OP_ADD,
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		};

		inline static VkPipelineColorBlendAttachmentState constexpr BLEND_ADDITIVE = {
			VK_TRUE,
			VK_BLEND_FACTOR_SRC_ALPHA,
			VK_BLEND_FACTOR_ONE,
			VK_BLEND_OP_ADD,
			VK_BLEND_FACTOR_SRC_ALPHA,
			VK_BLEND_FACTOR_ONE,
			VK_BLEND_OP_ADD,
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		};

		std::vector<VkPipelineColorBlendAttachmentState> blends;

		std::vector<std::pair<std::string, VkShaderStageFlagBits>> shaders;

		bool enable_depth_test  = true;
		bool enable_depth_write = true;
		bool enable_depth_bias = false;

		VkCompareOp depth_compare = VK_COMPARE_OP_LESS;

		VkPipelineLayout pipeline_layout;
		VkRenderPass     render_pass;
	};
	VkPipeline create_pipeline(PipelineDetails const & details);

	VkFramebuffer create_frame_buffer(int width, int height, VkRenderPass render_pass, std::vector<VkImageView> const & attachments);

	VkClearValue create_clear_value_colour(float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 0.0f);
	VkClearValue create_clear_value_depth (float depth = 1.0f, u32 stencil = 0);

	VkInstance get_instance();

	VkPhysicalDevice get_physical_device();
	VkDevice         get_device();

	VkSurfaceKHR get_surface();

	u32 get_queue_family_graphics();
	u32 get_queue_family_present();

	VkQueue get_queue_graphics();
	VkQueue get_queue_present();

	VkCommandPool get_command_pool();

	size_t get_min_uniform_buffer_alignment();
};
