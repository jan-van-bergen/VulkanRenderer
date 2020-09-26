#pragma once
#include <GLFW/glfw3.h>

namespace Input {
	bool is_key_down(int key); // Is Key currently down
	bool is_key_up  (int key); // Is key currently up

	bool is_key_pressed (int key); // Is Key currently down but up last frame
	bool is_key_released(int key); // Is Key currently up but down last frame
	
	void get_mouse_pos(int & x, int & y);
	void set_mouse_pos(int   x, int   y);

	void set_mouse_enabled(bool enabled);

	namespace detail {
		void init(GLFWwindow * window);

		void glfw_callback_key  (GLFWwindow * window, int key, int scancode, int action, int mods);
		void glfw_callback_mouse(GLFWwindow * window, double x, double y);

		void finish_frame();
	}
}
