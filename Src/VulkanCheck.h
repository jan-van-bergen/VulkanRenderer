#pragma once
#include <cstdio>

#include <vulkan/vulkan.h>

#define VK_CHECK(result) check_vulkan_call(result, __FILE__, __LINE__);

inline void check_vulkan_call(VkResult result, const char * file, int line) {
	if (result != VK_SUCCESS) {
		printf("Vulkan call at %s line %i failed!\n", file, line);

		__debugbreak();
	}
}
