#include "Shader.h"

#include "Util.h"
#include "VulkanCall.h"

Shader Shader::load(VkDevice device, std::string const & filename) {
	std::vector<char> spirv = read_file(filename);

	VkShaderModuleCreateInfo create_info = { };
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = spirv.size();
	create_info.pCode = reinterpret_cast<const uint32_t*>(spirv.data());

	Shader shader;
	VULKAN_CALL(vkCreateShaderModule(device, &create_info, nullptr, &shader.module));

	return shader;
}
