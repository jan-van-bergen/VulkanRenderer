#include "Camera.h"

#include "Input.h"

Camera::Camera(float fov, int width, int height, float near, float far) {
	this->fov = fov;

	this->near = near;
	this->far  = far;

	on_resize(width, height);
}

void Camera::on_resize(int width, int height) {
	projection = Matrix4::perspective(fov, static_cast<float>(width) / static_cast<float>(height), near, far);
}

void Camera::update(float delta) {
	const float MOVEMENT_SPEED = 10.0f;
	const float ROTATION_SPEED =  6.0f;

	Vector3 right   = rotation * Vector3(1.0f, 0.0f,  0.0f);
	Vector3 forward = rotation * Vector3(0.0f, 0.0f, -1.0f);

	if (Input::is_key_down(GLFW_KEY_W)) position += forward * MOVEMENT_SPEED * delta;
	if (Input::is_key_down(GLFW_KEY_A)) position -= right   * MOVEMENT_SPEED * delta;
	if (Input::is_key_down(GLFW_KEY_S)) position -= forward * MOVEMENT_SPEED * delta;
	if (Input::is_key_down(GLFW_KEY_D)) position += right   * MOVEMENT_SPEED * delta;

	if (Input::is_key_down(GLFW_KEY_LEFT_SHIFT)) position.y -= MOVEMENT_SPEED * delta;
	if (Input::is_key_down(GLFW_KEY_SPACE))      position.y += MOVEMENT_SPEED * delta;

	if (Input::is_key_down(GLFW_KEY_UP))    rotation = Quaternion::axis_angle(right,                     +ROTATION_SPEED * delta) * rotation;
	if (Input::is_key_down(GLFW_KEY_DOWN))  rotation = Quaternion::axis_angle(right,                     -ROTATION_SPEED * delta) * rotation;
	if (Input::is_key_down(GLFW_KEY_LEFT))  rotation = Quaternion::axis_angle(Vector3(0.0f, 1.0f, 0.0f), +ROTATION_SPEED * delta) * rotation;
	if (Input::is_key_down(GLFW_KEY_RIGHT)) rotation = Quaternion::axis_angle(Vector3(0.0f, 1.0f, 0.0f), -ROTATION_SPEED * delta) * rotation;

	view_projection = 
		projection *
		Matrix4::create_rotation(Quaternion::conjugate(rotation)) *
		Matrix4::create_translation(-position);
}
