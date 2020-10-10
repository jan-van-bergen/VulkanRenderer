#pragma once
#include "Vector3.h"
#include "Quaternion.h"
#include "Matrix4.h"

struct Transform {
	Vector3    position;
	Quaternion rotation;
	float      scale = 1.0f;

	inline Matrix4 get_matrix() const {
		return
			Matrix4::create_translation(position) *
			Matrix4::create_rotation(rotation) *
			Matrix4::create_scale(scale);
	}
};
