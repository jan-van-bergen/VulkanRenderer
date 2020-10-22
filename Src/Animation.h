#pragma once
#include <vector>
#include <unordered_map>

#include "Vector3.h"
#include "Quaternion.h"

struct Animation {
	struct KeyFramePosition { float time; Vector3    position; };
	struct KeyFrameRotation { float time; Quaternion rotation; };

	struct ChannelPosition {
		std::string name;
		std::vector<KeyFramePosition> key_frames;

		int current_frame = 0;

		Vector3 get_position(float time, bool loop);
	};

	struct ChannelRotation {
		std::string name;
		std::vector<KeyFrameRotation> key_frames;

		int current_frame = 0;

		Quaternion get_rotation(float time, bool loop);
	};

	std::vector<ChannelPosition> position_channels;
	std::vector<ChannelRotation> rotation_channels;
};
