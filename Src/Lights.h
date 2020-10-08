#pragma once
#include "Vector3.h"

#include "VulkanMemory.h"

struct Light {
	alignas(16) Vector3 colour;
};

struct DirectionalLight : Light {
	alignas(16) Vector3 direction;
};

struct PointLight : Light {
	alignas(16) Vector3 position;
	alignas(4)  float   radius;

	inline static struct Sphere {
		VulkanMemory::Buffer vertex_buffer;
		VulkanMemory::Buffer index_buffer;

		size_t index_count;
	} sphere;

	static void init_sphere();
	static void free_sphere();

};
