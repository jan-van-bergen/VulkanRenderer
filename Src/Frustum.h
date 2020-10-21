#pragma once
#include "Vector3.h"
#include "Matrix4.h"

struct Frustum {
	enum struct IntersectionType {
		FULLY_OUTSIDE,
		FULLY_INSIDE,
		INTERSECTING
	};

	struct Plane {
		Vector3 n;
		float   d;

		inline float distance(Vector3 const & point) const {
			return Vector3::dot(n, point) + d;
		}
	} planes[6];

	void from_matrix(Matrix4 const & view_projection);

	IntersectionType intersect_aabb(Vector3 const & min, Vector3 const & max) const;
	IntersectionType intersect_sphere(Vector3 const & center, float radius) const;
};
