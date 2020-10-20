#include "Animation.h"

#include <cmath>

void Animation::Channel::get_pose(float time, Vector3 * position, Quaternion * rotation) {
	assert(key_frames.size() > 0);

	auto total_length = key_frames[key_frames.size() - 1].time;

	if (time >= total_length) {
		time = std::fmodf(time, total_length);

		current_frame = 0;
	}

	while (time > key_frames[current_frame + 1].time) current_frame++;

	auto const & key_frame_curr = key_frames[current_frame];
	auto const & key_frame_next = key_frames[current_frame + 1];

	auto t = (time - key_frame_curr.time) / (key_frame_next.time - key_frame_curr.time);

	assert(0.0f <= t && t <= 1.0f);

	*position = Vector3::lerp    (key_frame_curr.position, key_frame_next.position, t);
	*rotation = Quaternion::nlerp(key_frame_curr.rotation, key_frame_next.rotation, t);
}
