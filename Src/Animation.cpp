#include "Animation.h"

#include <cmath>

Vector3 Animation::ChannelPosition::get_position(float time, bool loop) {
	assert(key_frames.size() > 0);

	auto total_length = key_frames[key_frames.size() - 1].time;

	if (time >= total_length && loop) {
		time = std::fmodf(time, total_length);

		current_frame = 0;
	}

	if (time < key_frames[0].time)                     return key_frames[0].position;
	if (time > key_frames[key_frames.size() - 1].time) return key_frames[key_frames.size() - 1].position;

	while (time > key_frames[current_frame + 1].time) current_frame++;

	auto const & key_frame_curr = key_frames[current_frame];
	auto const & key_frame_next = key_frames[current_frame + 1];

	auto t = (time - key_frame_curr.time) / (key_frame_next.time - key_frame_curr.time);

	assert(0.0f <= t && t <= 1.0f);

	return Vector3::lerp(key_frame_curr.position, key_frame_next.position, t);
}

Quaternion Animation::ChannelRotation::get_rotation(float time, bool loop) {
	assert(key_frames.size() > 0);

	auto total_length = key_frames[key_frames.size() - 1].time;

	if (time >= total_length && loop) {
		time = std::fmodf(time, total_length);

		current_frame = 0;
	}
	
	if (time < key_frames[0].time)                     return key_frames[0].rotation;
	if (time > key_frames[key_frames.size() - 1].time) return key_frames[key_frames.size() - 1].rotation;

	while (time > key_frames[current_frame + 1].time) current_frame++;

	auto const & key_frame_curr = key_frames[current_frame];
	auto const & key_frame_next = key_frames[current_frame + 1];

	auto t = (time - key_frame_curr.time) / (key_frame_next.time - key_frame_curr.time);

	assert(0.0f <= t && t <= 1.0f);

	return Quaternion::nlerp(key_frame_curr.rotation, key_frame_next.rotation, t);
}

void Animation::get_pose(std::string const & bone_name, float time, Vector3 * position, Quaternion * rotation, bool loop) {
	if (position_channels.find(bone_name) != position_channels.end()) {
		*position = position_channels[bone_name].get_position(time, loop);
	} else {
		*position = Vector3();
	}

	if (rotation_channels.find(bone_name) != rotation_channels.end()) {
		*rotation = rotation_channels[bone_name].get_rotation(time, loop);
	} else {
		*rotation = Quaternion();
	}
}
