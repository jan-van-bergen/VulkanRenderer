#include "Shader.h"

#include <vector>

#include "VulkanCheck.h"
#include "Util.h"

Shader::Shader(VkDevice device, std::string const & filename, VkShaderStageFlagBits stage) {
	std::vector<char> spirv = Util::read_file(filename);

	VkShaderModuleCreateInfo create_info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	create_info.codeSize = spirv.size();
	create_info.pCode = reinterpret_cast<const u32 *>(spirv.data());

	VK_CHECK(vkCreateShaderModule(device, &create_info, nullptr, &shader));
}
