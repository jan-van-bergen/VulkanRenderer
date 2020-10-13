#pragma once
#include "Matrix4.h"

#include "VulkanMemory.h"
#include "RenderTarget.h"

struct Light {
	Vector3 colour;
};

struct DirectionalLight : Light {
private:
	inline static auto const projection = Matrix4::orthographic(200.0f, 200.0f, -200.0f, 200.0f);

public:
	Vector3 direction;

	struct {
		RenderTarget  render_target;
		VkFramebuffer frame_buffer;
		VkSampler     sampler;

		VkDescriptorSet descriptor_set;
	} shadow_map;

	Matrix4 get_light_matrix() const {
		return projection * Matrix4::look_at(Vector3(0.0f), direction, Vector3(0.0f, 1.0f, 0.0f));
	}
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
