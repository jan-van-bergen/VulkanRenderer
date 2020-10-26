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

	if (locked) Input::get_mouse_pos(&mouse_prev_x, &mouse_prev_y);
}

void Camera::on_resize(int width, int height) {
	this->width  = float(width);
	this->height = float(height);

	float aspect = this->width / this->height;

	projection     = Matrix4::perspective    (fov, aspect, near, far);
	projection_inv = Matrix4::perspective_inv(fov, aspect, near, far);

	top_left_corner = Vector3(-0.5f * this->width,  0.5f * this->height, -far);
}

void Camera::update(float delta) {
	const float MOVEMENT_SPEED = 15.0f;
	const float ROTATION_SPEED =  5.0f;

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
		int mouse_x, mouse_y; Input::get_mouse_pos(&mouse_x, &mouse_y);
		
		angle_x -= delta * float(mouse_x - mouse_prev_x);
		angle_y -= delta * float(mouse_y - mouse_prev_y);

		// Clamp vertical rotation between 90 degrees down and 90 degrees up
		angle_y = Math::clamp(angle_y, -0.499f * PI, 0.499f * PI);

		mouse_prev_x = mouse_x;
		mouse_prev_y = mouse_y;
	}
	
	// For debugging purposes
	if (Input::is_key_pressed(GLFW_KEY_F)) {
		printf("camera.position = Vector3(%ff, %ff, %ff);\n", position.x, position.y, position.z);
		printf("camera.angle_x = %ff;\n", angle_x);
		printf("camera.angle_y = %ff;\n", angle_y);
	}

	rotation =
		Quaternion::axis_angle(Vector3(0.0f, 1.0f, 0.0f), angle_x) *
		Quaternion::axis_angle(Vector3(1.0f, 0.0f, 0.0f), angle_y);

	view_projection = 
		projection *
		Matrix4::create_rotation(Quaternion::conjugate(rotation)) *
		Matrix4::create_translation(-position);

	view_projection_inv = 
		Matrix4::create_translation(position) *
		Matrix4::create_rotation(rotation) *
		projection_inv;

	frustum.from_matrix(view_projection);
}
