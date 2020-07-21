#pragma once
#include <vulkan/vulkan.h>

#include <string>

class Shader {
public:
	VkShaderModule module;

	static Shader load(VkDevice device, std::string const & filename);

	inline VkPipelineShaderStageCreateInfo get_stage(VkShaderStageFlagBits stage) const {
		VkPipelineShaderStageCreateInfo vertex_stage_create_info = { };
		vertex_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertex_stage_create_info.stage = stage;

		vertex_stage_create_info.module = module;
		vertex_stage_create_info.pName = "main";

		return vertex_stage_create_info;
	}

	inline void destroy(VkDevice device) {
		vkDestroyShaderModule(device, module, nullptr);
	}
};
