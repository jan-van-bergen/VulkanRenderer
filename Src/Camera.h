#pragma once
#include "Matrix4.h"

class Camera {
	float fov; // Field of View in radians

	float near;
	float far;

	Matrix4      projection;
	Matrix4 view_projection;
	
	bool mouse_locked;

	float angle_x;
	float angle_y;

	int mouse_prev_x;
	int mouse_prev_y;

	void set_locked(bool locked);

public:
	Vector3    position;
	Quaternion rotation;
	
	Vector3 top_left_corner;

	Camera(float fov, int width, int height, float near = 0.1f, float far = 500.0f);
	
	void on_resize(int width, int height);

	void update(float delta);

	inline Matrix4 const & get_view_projection() const { return view_projection; }
};
