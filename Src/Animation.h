#pragma once
#include <vector>
#include <unordered_map>

#include "Vector3.h"
#include "Quaternion.h"

struct Animation {
	struct KeyFramePosition { float time; Vector3    position; };
	struct KeyFrameRotation { float time; Quaternion rotation; };

	struct ChannelPosition {
		std::vector<KeyFramePosition> key_frames;

		int current_frame = 0;

		Vector3 get_position(float time);
	};

	struct ChannelRotation {
		std::vector<KeyFrameRotation> key_frames;

		int current_frame = 0;

		Quaternion get_rotation(float time);
	};

	std::unordered_map<std::string, ChannelPosition> position_channels;
	std::unordered_map<std::string, ChannelRotation> rotation_channels;

	void get_pose(std::string const & bone_name, float time, Vector3 * position, Quaternion * rotation);
};
