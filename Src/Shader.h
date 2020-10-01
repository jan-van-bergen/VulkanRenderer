#pragma once
#include <string>

#include <vulkan/vulkan.h>

class Shader {
	VkShaderModule shader;

public:
	Shader(VkDevice device, std::string const & filename, VkShaderStageFlagBits stage);
};
