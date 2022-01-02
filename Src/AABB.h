#pragma once
#include <algorithm>

#include "Vector3.h"

struct AABB {
	Vector3 min;
	Vector3 max;

	bool intersects_ray(Vector3 const & origin, Vector3 const & direction, float max_distance = INFINITY) const {
		Vector3 inv_direction(
			1.0f / direction.x,
			1.0f / direction.y,
			1.0f / direction.z
		);

		Vector3 t0 = (min - origin) * inv_direction;
		Vector3 t1 = (max - origin) * inv_direction;

		Vector3 t_min = Vector3::min(t0, t1);
		Vector3 t_max = Vector3::max(t0, t1);

		float t_near = std::max(std::max(0.001f,       t_min.x), std::max(t_min.y, t_min.z));
		float t_far  = std::min(std::min(max_distance, t_max.x), std::min(t_max.y, t_max.z));

		return t_near < t_far;
	}
};
