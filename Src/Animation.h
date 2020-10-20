#pragma once
#include <vector>
#include <unordered_map>

#include "Vector3.h"
#include "Quaternion.h"

struct Animation {
	struct KeyFrame {
		float time;

		Vector3    position;
		Quaternion rotation;
	};

	struct Channel {
		std::vector<KeyFrame> key_frames;

		int current_frame = 0;

		void get_pose(float time, Vector3 * position, Quaternion * rotation);
	};

	std::unordered_map<std::string, Channel> channels;
};
