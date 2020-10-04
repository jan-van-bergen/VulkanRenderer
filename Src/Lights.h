#pragma once
#include "Vector3.h"

#include "VulkanMemory.h"

struct DirectionalLight {
	Vector3 colour;

	Vector3 direction;
};

struct PointLight {
	inline static struct Sphere {
		VulkanMemory::Buffer vertex_buffer;
		VulkanMemory::Buffer index_buffer;

		size_t index_count;
	} sphere;

	static void init_sphere();
	static void free_sphere();

	Vector3 colour;

	Vector3 position;
	float   radius;
};
