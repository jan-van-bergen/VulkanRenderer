#pragma once
#include "Vector3.h"

#include "VulkanMemory.h"

struct Light {
	Vector3 colour;
};

struct DirectionalLight : Light {
	Vector3 direction;
};

struct PointLight : Light {
	Vector3 position;
	float   radius;

	inline static struct Sphere {
		VulkanMemory::Buffer vertex_buffer;
		VulkanMemory::Buffer index_buffer;

		size_t index_count;
	} sphere;

	static void init_sphere();
	static void free_sphere();
};

struct SpotLight : PointLight {
	Vector3 direction;

	float cutoff_inner;
	float cutoff_outer;
};
