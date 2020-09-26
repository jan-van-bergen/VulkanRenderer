#pragma once
#include <vulkan/vulkan.h>

struct Material {
	VkPipeline       pipeline;
	VkPipelineLayout pipeline_layout;
};
