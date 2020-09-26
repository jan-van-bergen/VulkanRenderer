#include "Input.h"

#include <string.h>

#define KEY_TABLE_SIZE GLFW_KEY_LAST

static GLFWwindow * window;

static bool keyboard_state_curr[KEY_TABLE_SIZE] = { };
static bool keyboard_state_prev[KEY_TABLE_SIZE] = { };

static int mouse_x;
static int mouse_y;

bool Input::is_key_down(int key) {
	return keyboard_state_curr[key];
}

bool Input::is_key_up(int key) {
	return !keyboard_state_curr[key];
}

bool Input::is_key_pressed(int key) {
	return keyboard_state_curr[key] && !keyboard_state_prev[key];
}

bool Input::is_key_released(int key) {
	return !keyboard_state_curr[key] && keyboard_state_prev[key];
}

void Input::get_mouse_pos(int & x, int & y) {
	x = mouse_x;
	y = mouse_y;
}

void Input::set_mouse_pos(int x, int y) {
	glfwSetCursorPos(window, double(x), double(y));
}

void Input::set_mouse_enabled(bool enabled) {
	glfwSetInputMode(window, GLFW_CURSOR, enabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
}

void Input::detail::init(GLFWwindow * window) {
	::window = window;
}

void Input::detail::glfw_callback_key(GLFWwindow * window, int key, int scancode, int action, int mods) {
	if (key < 0 || key >= KEY_TABLE_SIZE) return;

	switch (action) {
		case GLFW_PRESS:   keyboard_state_curr[key] = true;  break;
		case GLFW_RELEASE: keyboard_state_curr[key] = false; break;
	}
}

void Input::detail::glfw_callback_mouse(GLFWwindow * window, double x, double y) {
	mouse_x = int(x);
	mouse_y = int(y);
}

void Input::detail::finish_frame() {
	memcpy(keyboard_state_prev, keyboard_state_curr, KEY_TABLE_SIZE);
}
