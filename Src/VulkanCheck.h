#pragma once
#include <cstdio>

#include <vulkan/vulkan.h>

#define VK_CHECK(result) check_vulkan_call(result, __FILE__, __LINE__);

inline char const * vulkan_error_string(VkResult error) {
		switch (error) {
#define CASE_ERROR(c) case c: return #c
            CASE_ERROR(VK_NOT_READY);
            CASE_ERROR(VK_TIMEOUT);
            CASE_ERROR(VK_EVENT_SET);
            CASE_ERROR(VK_EVENT_RESET);
            CASE_ERROR(VK_INCOMPLETE);
            CASE_ERROR(VK_ERROR_OUT_OF_HOST_MEMORY);
            CASE_ERROR(VK_ERROR_OUT_OF_DEVICE_MEMORY);
            CASE_ERROR(VK_ERROR_INITIALIZATION_FAILED);
            CASE_ERROR(VK_ERROR_DEVICE_LOST);
            CASE_ERROR(VK_ERROR_MEMORY_MAP_FAILED);
            CASE_ERROR(VK_ERROR_LAYER_NOT_PRESENT);
            CASE_ERROR(VK_ERROR_EXTENSION_NOT_PRESENT);
            CASE_ERROR(VK_ERROR_FEATURE_NOT_PRESENT);
            CASE_ERROR(VK_ERROR_INCOMPATIBLE_DRIVER);
            CASE_ERROR(VK_ERROR_TOO_MANY_OBJECTS);
            CASE_ERROR(VK_ERROR_FORMAT_NOT_SUPPORTED);
            CASE_ERROR(VK_ERROR_FRAGMENTED_POOL);
            CASE_ERROR(VK_ERROR_UNKNOWN);
            CASE_ERROR(VK_ERROR_OUT_OF_POOL_MEMORY);
            CASE_ERROR(VK_ERROR_INVALID_EXTERNAL_HANDLE);
            CASE_ERROR(VK_ERROR_FRAGMENTATION);
            CASE_ERROR(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS);
            CASE_ERROR(VK_ERROR_SURFACE_LOST_KHR);
            CASE_ERROR(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
            CASE_ERROR(VK_SUBOPTIMAL_KHR);
            CASE_ERROR(VK_ERROR_OUT_OF_DATE_KHR);
            CASE_ERROR(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
            CASE_ERROR(VK_ERROR_VALIDATION_FAILED_EXT);
            CASE_ERROR(VK_ERROR_INVALID_SHADER_NV);
            CASE_ERROR(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
            CASE_ERROR(VK_ERROR_NOT_PERMITTED_EXT);
            CASE_ERROR(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
            CASE_ERROR(VK_THREAD_IDLE_KHR);
            CASE_ERROR(VK_THREAD_DONE_KHR);
            CASE_ERROR(VK_OPERATION_DEFERRED_KHR);
            CASE_ERROR(VK_OPERATION_NOT_DEFERRED_KHR);
            CASE_ERROR(VK_PIPELINE_COMPILE_REQUIRED_EXT);
#undef STR
			default: return "Unknown Erorr!";
		}
}

inline void check_vulkan_call(VkResult result, const char * file, int line) {
	if (result != VK_SUCCESS) {
		printf("Vulkan call at %s line %i failed with error: %s!\n", file, line, vulkan_error_string(result));

		__debugbreak();
	}
}
