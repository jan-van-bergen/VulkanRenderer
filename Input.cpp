#include "Input.h"

#include <string.h>

#define KEY_TABLE_SIZE GLFW_KEY_LAST

static bool keyboard_state_curr[KEY_TABLE_SIZE] = { };
static bool keyboard_state_prev[KEY_TABLE_SIZE] = { };

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

void Input::update_key(int key, int action) {
	switch (action) {
		case GLFW_PRESS:   keyboard_state_curr[key] = true;  break;
		case GLFW_RELEASE: keyboard_state_curr[key] = false; break;
	}
}

void Input::finish_frame() {
	memcpy(keyboard_state_prev, keyboard_state_curr, KEY_TABLE_SIZE);
}
