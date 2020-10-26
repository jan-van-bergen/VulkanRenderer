#pragma once
#include "Frustum.h"

struct Camera {
private:
	float fov; // Field of View in radians

	float width, height;

	float near;
	float far;

	Matrix4 projection;
	Matrix4 projection_inv;
	Matrix4 view_projection;
	Matrix4 view_projection_inv;
	
	Vector3 top_left_corner;

	bool mouse_locked;

	int mouse_prev_x;
	int mouse_prev_y;

	void set_locked(bool locked);

public:
	Vector3    position;
	Quaternion rotation;
	
	float angle_x;
	float angle_y;

	Frustum frustum;

	Camera(float fov, int width, int height, float near = 0.1f, float far = 500.0f);
	
	void on_resize(int width, int height);

	void update(float delta);

	inline Vector3 get_ray_direction(int x, int y) const {
		return rotation * Vector3::normalize(top_left_corner + 
			float(x) * Vector3(1.0f,  0.0f, 0.0f) +
			float(y) * Vector3(0.0f, -1.0f, 0.0f)
		);
	}

	inline Vector3 get_top_left_corner() const { return rotation * top_left_corner; }
	inline Vector3 get_x_axis()          const { return rotation * Vector3(float(width), 0.0f, 0.0f); }
	inline Vector3 get_y_axis()          const { return rotation * Vector3(0.0f, -float(height), 0.0f); }
	
	inline Matrix4 const & get_view_projection()     const { return view_projection; }
	inline Matrix4 const & get_inv_view_projection() const { return view_projection_inv; }

};
