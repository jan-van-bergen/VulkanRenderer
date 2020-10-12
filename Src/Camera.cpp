#include "Camera.h"

#include "Input.h"

Camera::Camera(float fov, int width, int height, float near, float far) {
	this->fov = fov;

	this->near = near;
	this->far  = far;

	set_locked(false);
	
	angle_x = 0.0f;
	angle_y = 0.0f;
	
	on_resize(width, height);
}

void Camera::set_locked(bool locked) {
	mouse_locked = locked;

	Input::set_mouse_enabled(!locked);

	if (locked) Input::get_mouse_pos(mouse_prev_x, mouse_prev_y);
}

void Camera::Frustum::from_view_projection(Matrix4 const & view_projection) {
	// Based on: "Fast Extraction of Viewing Frustum Planes from the World-View-Projection Matrix" by Gil Gribb and Klaus Hartmann
	auto calc_plane = [&](Plane * plane, int row_0, int row_1, bool neg) {
		float A = (neg ? -1.0f : +1.0f) * view_projection(row_0, 0) + view_projection(row_1, 0);
		float B = (neg ? -1.0f : +1.0f) * view_projection(row_0, 1) + view_projection(row_1, 1);
		float C = (neg ? -1.0f : +1.0f) * view_projection(row_0, 2) + view_projection(row_1, 2);
		float D = (neg ? -1.0f : +1.0f) * view_projection(row_0, 3) + view_projection(row_1, 3);

		float normalization_factor = 1.0f / sqrtf(A*A + B*B + C*C);

		plane->n = normalization_factor * Vector3(A, B, C);
		plane->d = normalization_factor * D;
	};

	calc_plane(&planes[0], 2, 3, false); // Near
	calc_plane(&planes[1], 2, 3, true);  // Far
	calc_plane(&planes[2], 1, 3, false); // Bottom
	calc_plane(&planes[3], 1, 3, true);  // Top
	calc_plane(&planes[4], 0, 3, false); // Left
	calc_plane(&planes[5], 0, 3, true);  // Right
}

Camera::Frustum::IntersectionType Camera::Frustum::intersect_aabb(Vector3 const & min, Vector3 const & max) const {
	Vector3 corners[8] = {
		Vector3(min.x, min.y, min.z),
		Vector3(min.x, min.y, max.z),
		Vector3(max.x, min.y, max.z),
		Vector3(max.x, min.y, min.z),
		Vector3(min.x, max.y, min.z),
		Vector3(min.x, max.y, max.z),
		Vector3(max.x, max.y, max.z),
		Vector3(max.x, max.y, min.z)
	};

	int num_valid_planes = 0;

	// For each plane, test all 8 corners of the AABB
	for (int p = 0; p < 6; p++) {
		int num_inside = 0;
		
		for (int i = 0; i < 8; i++) {
			if (planes[p].distance(corners[i]) > 0.0f) {
				num_inside++;
			}
		}

		// If all 8 corners were outside the current plane, we are fully outside the frustum
		if (num_inside == 0) return IntersectionType::FULLY_OUTSIDE;

		num_valid_planes += (num_inside == 8);
	}

	// If all planes were valid, we are fully inside the frustum
	if (num_valid_planes == 6) return IntersectionType::FULLY_INSIDE;

	return IntersectionType::INTERSECTING;
}

Camera::Frustum::IntersectionType Camera::Frustum::intersect_sphere(Vector3 const & center, float radius) const {
	for (int i = 0; i < 6; i++) {
		float distance = planes[i].distance(center);

		if (distance < -radius) return IntersectionType::FULLY_OUTSIDE;
		if (distance <  radius) return IntersectionType::INTERSECTING;
	}

	return IntersectionType::FULLY_INSIDE;
}

void Camera::on_resize(int width, int height) {
	projection = Matrix4::perspective(fov, static_cast<float>(width) / static_cast<float>(height), near, far);

	top_left_corner = Vector3(-0.5f * float(width),  0.5f * float(height), -far);
}

void Camera::update(float delta) {
	const float MOVEMENT_SPEED = 10.0f;
	const float ROTATION_SPEED =  1.0f;

	Vector3 right   = rotation * Vector3(1.0f, 0.0f,  0.0f);
	Vector3 forward = rotation * Vector3(0.0f, 0.0f, -1.0f);

	if (Input::is_key_down(GLFW_KEY_W)) position += forward * MOVEMENT_SPEED * delta;
	if (Input::is_key_down(GLFW_KEY_A)) position -= right   * MOVEMENT_SPEED * delta;
	if (Input::is_key_down(GLFW_KEY_S)) position -= forward * MOVEMENT_SPEED * delta;
	if (Input::is_key_down(GLFW_KEY_D)) position += right   * MOVEMENT_SPEED * delta;

	if (Input::is_key_down(GLFW_KEY_LEFT_SHIFT)) position.y -= MOVEMENT_SPEED * delta;
	if (Input::is_key_down(GLFW_KEY_SPACE))      position.y += MOVEMENT_SPEED * delta;
	
	// Toggle mouse lock
	if (Input::is_key_pressed(GLFW_KEY_ESCAPE)) set_locked(!mouse_locked);

	// Manual Camera rotation using keys
	if (Input::is_key_down(GLFW_KEY_UP))    angle_y += ROTATION_SPEED * delta;
	if (Input::is_key_down(GLFW_KEY_DOWN))  angle_y -= ROTATION_SPEED * delta;
	if (Input::is_key_down(GLFW_KEY_LEFT))  angle_x += ROTATION_SPEED * delta;
	if (Input::is_key_down(GLFW_KEY_RIGHT)) angle_x -= ROTATION_SPEED * delta;
	
	// If mouse is locked rotate Camera based on mouse movement
	if (mouse_locked) {
		int mouse_x, mouse_y; Input::get_mouse_pos(mouse_x, mouse_y);
		
		angle_x -= ROTATION_SPEED * delta * float(mouse_x - mouse_prev_x);
		angle_y -= ROTATION_SPEED * delta * float(mouse_y - mouse_prev_y);

		// Clamp vertical rotation between 90 degrees down and 90 degrees up
		angle_y = Math::clamp(angle_y, -0.499f * PI, 0.499f * PI);

		mouse_prev_x = mouse_x;
		mouse_prev_y = mouse_y;
	}
	
	rotation =
		Quaternion::axis_angle(Vector3(0.0f, 1.0f, 0.0f), angle_x) *
		Quaternion::axis_angle(Vector3(1.0f, 0.0f, 0.0f), angle_y);

	view_projection = 
		projection *
		Matrix4::create_rotation(Quaternion::conjugate(rotation)) *
		Matrix4::create_translation(-position);

	frustum.from_view_projection(view_projection);
}
