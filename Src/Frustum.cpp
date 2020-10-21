#include "Frustum.h"

#include <immintrin.h>

void Frustum::from_matrix(Matrix4 const & matrix) {
	// Based on: "Fast Extraction of Viewing Frustum Planes from the World-View-Projection Matrix" by Gil Gribb and Klaus Hartmann
	auto calc_plane = [&](Plane * plane, int row_0, int row_1, bool neg) {
		float A = (neg ? -1.0f : +1.0f) * matrix(row_0, 0) + matrix(row_1, 0);
		float B = (neg ? -1.0f : +1.0f) * matrix(row_0, 1) + matrix(row_1, 1);
		float C = (neg ? -1.0f : +1.0f) * matrix(row_0, 2) + matrix(row_1, 2);
		float D = (neg ? -1.0f : +1.0f) * matrix(row_0, 3) + matrix(row_1, 3);

		float normalization_factor = 1.0f / sqrtf(A*A + B*B + C*C);

		plane->n = normalization_factor * Vector3(A, B, C);
		plane->d = normalization_factor * D;
	};

	calc_plane(&planes[0], 2, 3, false); // Near
	calc_plane(&planes[1], 2, 3, true);  // Far
	calc_plane(&planes[2], 1, 3, false); // Bottom
	calc_plane(&planes[3], 1, 3, true);  // Top
	calc_plane(&planes[4], 0, 3, false); // Left
	calc_plane(&planes[5], 0, 3, true);  // Right
}

Frustum::IntersectionType Frustum::intersect_aabb(Vector3 const & min, Vector3 const & max) const {
#if 0
	Vector3 corners[8] = {
		Vector3(min.x, min.y, min.z),
		Vector3(min.x, min.y, max.z),
		Vector3(max.x, min.y, max.z),
		Vector3(max.x, min.y, min.z),
		Vector3(min.x, max.y, min.z),
		Vector3(min.x, max.y, max.z),
		Vector3(max.x, max.y, max.z),
		Vector3(max.x, max.y, min.z)
	};

	__m256 corners_x = _mm256_setr_ps(min.x, min.x, max.x, max.x, min.x, min.x, max.x, max.x);
	__m256 corners_y = _mm256_setr_ps(min.y, min.y, min.y, min.y, max.y, max.y, max.y, max.y);
	__m256 corners_z = _mm256_setr_ps(min.z, max.z, max.z, min.z, min.z, max.z, max.z, min.z);

	int num_valid_planes = 0;

	// For each plane, test all 8 corners of the AABB
	for (int p = 0; p < 6; p++) {
		int num_inside = 0;
		
		__m256 plane_a = _mm256_set1_ps(planes[p].n.x);
		__m256 plane_b = _mm256_set1_ps(planes[p].n.y);
		__m256 plane_c = _mm256_set1_ps(planes[p].n.z);
		__m256 plane_d = _mm256_set1_ps(planes[p].d);

		for (int i = 0; i < 8; i++) {
			if (planes[p].distance(corners[i]) > 0.0f) {
				num_inside++;
			}
		}

		// If all 8 corners were outside the current plane, we are fully outside the frustum
		if (num_inside == 0) return IntersectionType::FULLY_OUTSIDE;

		num_valid_planes += (num_inside == 8);
	}

	// If all planes were valid, we are fully inside the frustum
	if (num_valid_planes == 6) return IntersectionType::FULLY_INSIDE;

	return IntersectionType::INTERSECTING;
#else
	__m256 const zero = _mm256_set1_ps(0.0f);

	__m256 corners_x = _mm256_setr_ps(min.x, min.x, max.x, max.x, min.x, min.x, max.x, max.x);
	__m256 corners_y = _mm256_setr_ps(min.y, min.y, min.y, min.y, max.y, max.y, max.y, max.y);
	__m256 corners_z = _mm256_setr_ps(min.z, max.z, max.z, min.z, min.z, max.z, max.z, min.z);

	int num_valid_planes = 0;

	// For each plane, test all 8 corners of the AABB
	for (int p = 0; p < 6; p++) {
		// Load plane
		__m256 plane_a = _mm256_set1_ps(planes[p].n.x);
		__m256 plane_b = _mm256_set1_ps(planes[p].n.y);
		__m256 plane_c = _mm256_set1_ps(planes[p].n.z);
		__m256 plane_d = _mm256_set1_ps(planes[p].d);

		// Calculate signed distance to plane
		__m256 distance = _mm256_add_ps(
			_mm256_add_ps(
				_mm256_mul_ps(plane_a, corners_x), _mm256_mul_ps(plane_b, corners_y)
			),
			_mm256_add_ps(
				_mm256_mul_ps(plane_c, corners_z), plane_d
			)
		);

		// Check if signed distance is positive
		int mask_inside = _mm256_movemask_ps(_mm256_cmp_ps(distance, zero, _CMP_GT_OQ));

		// If all 8 corners were outside the current plane, we are fully outside the frustum
		if (mask_inside == 0) return IntersectionType::FULLY_OUTSIDE;

		// If all 8 corners were inside the current plane, the plane is valid
		num_valid_planes += (mask_inside == 0xff);
	}

	// If all planes were valid, we are fully inside the frustum
	if (num_valid_planes == 6) return IntersectionType::FULLY_INSIDE;

	return IntersectionType::INTERSECTING;
#endif
}

Frustum::IntersectionType Frustum::intersect_sphere(Vector3 const & center, float radius) const {
	for (int i = 0; i < 6; i++) {
		float distance = planes[i].distance(center);

		if (distance < -radius) return IntersectionType::FULLY_OUTSIDE;
		if (distance <  radius) return IntersectionType::INTERSECTING;
	}

	return IntersectionType::FULLY_INSIDE;
}
