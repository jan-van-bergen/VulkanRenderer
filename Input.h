#pragma once
#include <GLFW/glfw3.h>

namespace Input {
	bool is_key_down(int key); // Is Key currently down
	bool is_key_up  (int key); // Is key currently up

	bool is_key_pressed (int key); // Is Key currently down but up last frame
	bool is_key_released(int key); // Is Key currently up but down last frame
	
	void update_key(int key, int action);

	void finish_frame();
}
