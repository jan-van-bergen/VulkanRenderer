#pragma once
#include "Matrix4.h"

class Camera {
	float fov; // Field of View in radians

	float near;
	float far;

	Matrix4      projection;
	Matrix4 view_projection;

public:
	Vector3    position;
	Quaternion rotation;

	Camera(float fov, int width, int height, float near = 0.1f, float far = 100.0f);
	
	void on_resize(int width, int height);

	void update(float delta);

	Matrix4 const & get_view_projection() const { return view_projection; }
};
