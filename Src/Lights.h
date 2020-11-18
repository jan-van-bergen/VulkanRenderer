#pragma once
#include "Matrix4.h"

#include "VulkanMemory.h"
#include "RenderTarget.h"

struct Light {
	Vector3 colour;
};

struct DirectionalLight : Light {
private:
	inline static auto const projection = Matrix4::orthographic(200.0f, 200.0f, -200.0f, 50.0f);

public:
	Quaternion orientation;

	struct {
		RenderTarget render_target;

		VkDescriptorSet descriptor_set;
	} shadow_map;

	Vector3 get_direction() const {
		return orientation * Vector3(0.0f, 0.0f, -1.0f);
	}

	Matrix4 get_light_matrix() const {
		return projection * Matrix4::create_rotation(Quaternion::conjugate(orientation));
	}
};

struct PointLight : Light {
	Vector3 position;
	float   radius;
};

struct SpotLight : PointLight {
	Vector3 direction;

	float cutoff_inner;
	float cutoff_outer;
};
