#include "Gizmo.h"

#include "Util.h"

static constexpr int VERTICES_PER_CIRCLE = 12;

void Gizmo::calc_aabb() {
	aabb.min = Vector3(+INFINITY);
	aabb.max = Vector3(-INFINITY);
	
	for (auto const & vertex : vertices) {
		aabb.min = Vector3::min(aabb.min, vertex.position);
		aabb.max = Vector3::max(aabb.max, vertex.position);
	}
}

Gizmo Gizmo::generate_position() {
	constexpr float RADIUS_TAIL = 0.1f;
	constexpr float RADIUS_HEAD = 0.25f;
	constexpr float LENGTH_TAIL = 2.0f;
	constexpr float LENTGH_HEAD = 0.5f;
	
	Gizmo gizmo;

	gizmo.vertices = { { Vector3(0, 0, -1) }, { Vector3(3, 0, -2) }, { Vector3(2, 2, -1) } };
	gizmo.indices = { 0, 1, 2 };

	/*
	gizmo.vertices.resize(3 * VERTICES_PER_CIRCLE + 2);

	gizmo.vertices[0].position = Vector3(0.0f, 0.0f, 0.0f);

	for (int i = 0; i < VERTICES_PER_CIRCLE; i++) {
		auto angle = TWO_PI * float(i) / float(VERTICES_PER_CIRCLE);
		auto x = std::cos(angle);
		auto y = std::sin(angle);

		gizmo.vertices[i + 1].position                           = Vector3(RADIUS_TAIL * x, RADIUS_TAIL * y, 0.0f);
		gizmo.vertices[i + 1 + VERTICES_PER_CIRCLE]    .position = Vector3(RADIUS_TAIL * x, RADIUS_TAIL * y, -LENGTH_TAIL);
		gizmo.vertices[i + 1 + VERTICES_PER_CIRCLE * 2].position = Vector3(RADIUS_HEAD * x, RADIUS_HEAD * y, -LENGTH_TAIL);
	}

	gizmo.vertices[1 + 3 * VERTICES_PER_CIRCLE].position = Vector3(0.0f, 0.0f, -(LENGTH_TAIL + LENTGH_HEAD));

	// Backside of tail
	for (int i = 1; i < VERTICES_PER_CIRCLE; i++) {
		gizmo.indices.push_back(0);
		gizmo.indices.push_back(i);
		gizmo.indices.push_back(i + 1);
	}

	gizmo.indices.push_back(0);
	gizmo.indices.push_back(VERTICES_PER_CIRCLE);
	gizmo.indices.push_back(1);

	// Edge loop that connects tail to Head
	for (int i = 1; i < VERTICES_PER_CIRCLE; i++) {
		gizmo.indices.push_back(i);
		gizmo.indices.push_back(i + VERTICES_PER_CIRCLE);
		gizmo.indices.push_back(i + VERTICES_PER_CIRCLE + 1);

		gizmo.indices.push_back(i);
		gizmo.indices.push_back(i + VERTICES_PER_CIRCLE + 1);
		gizmo.indices.push_back(i + 1);
	}

	gizmo.indices.push_back(VERTICES_PER_CIRCLE);
	gizmo.indices.push_back(VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE);
	gizmo.indices.push_back(VERTICES_PER_CIRCLE + 1);

	gizmo.indices.push_back(VERTICES_PER_CIRCLE);
	gizmo.indices.push_back(VERTICES_PER_CIRCLE + 1);
	gizmo.indices.push_back(1);

	// Outer loop of head
	for (int i = 1 + VERTICES_PER_CIRCLE; i < 2 * VERTICES_PER_CIRCLE; i++) {
		gizmo.indices.push_back(i);
		gizmo.indices.push_back(i + VERTICES_PER_CIRCLE);
		gizmo.indices.push_back(i + VERTICES_PER_CIRCLE + 1);

		gizmo.indices.push_back(i);
		gizmo.indices.push_back(i + VERTICES_PER_CIRCLE + 1);
		gizmo.indices.push_back(i + 1);
	}

	gizmo.indices.push_back(VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE);
	gizmo.indices.push_back(VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE);
	gizmo.indices.push_back(VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE + 1);

	gizmo.indices.push_back(VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE);
	gizmo.indices.push_back(VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE + 1);
	gizmo.indices.push_back(VERTICES_PER_CIRCLE + 1);

	// Connect outer loop of head to tip of head
	auto last = gizmo.vertices.size() - 1;

	for (int i = 1 + VERTICES_PER_CIRCLE * 2; i < 3 * VERTICES_PER_CIRCLE; i++) {
		gizmo.indices.push_back(i);
		gizmo.indices.push_back(last);
		gizmo.indices.push_back(i + 1);
	}

	gizmo.indices.push_back(3 * VERTICES_PER_CIRCLE);
	gizmo.indices.push_back(last);
	gizmo.indices.push_back(2 * VERTICES_PER_CIRCLE + 1);
	*/

	auto buffer_size_vertices = Util::vector_size_in_bytes(gizmo.vertices);
	auto buffer_size_indices  = Util::vector_size_in_bytes(gizmo.indices);

	gizmo.vertex_buffer = VulkanMemory::buffer_create(buffer_size_vertices, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	gizmo.index_buffer  = VulkanMemory::buffer_create(buffer_size_indices,  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VulkanMemory::buffer_copy_staged(gizmo.vertex_buffer, gizmo.vertices.data(), buffer_size_vertices);
	VulkanMemory::buffer_copy_staged(gizmo.index_buffer,  gizmo.indices .data(), buffer_size_indices);

	gizmo.calc_aabb();

	return gizmo;
}

Gizmo Gizmo::generate_rotation() {
	constexpr float RADIUS_INNER = 2.0f;
	constexpr float RADIUS_OUTER = 2.2f;
	constexpr float HALF_DEPTH   = 0.1f;
	
	Gizmo gizmo;
	gizmo.vertices.resize(4 * VERTICES_PER_CIRCLE);
	gizmo.indices.reserve(4 * 2 * 3 * VERTICES_PER_CIRCLE); // 4 sides, each with 2 triangles

	// Generate Vertices
	std::vector<Vector3> vertices();
	
	for (int i = 0; i < VERTICES_PER_CIRCLE; i++) {
		auto angle = TWO_PI * float(i) / float(VERTICES_PER_CIRCLE);
		auto x = std::cos(angle);
		auto y = std::sin(angle);

		gizmo.vertices[i].position                           = Vector3(RADIUS_INNER * x, RADIUS_INNER * y,  HALF_DEPTH);
		gizmo.vertices[i + 1 * VERTICES_PER_CIRCLE].position = Vector3(RADIUS_INNER * x, RADIUS_INNER * y, -HALF_DEPTH);
		gizmo.vertices[i + 2 * VERTICES_PER_CIRCLE].position = Vector3(RADIUS_OUTER * x, RADIUS_OUTER * y, -HALF_DEPTH);
		gizmo.vertices[i + 3 * VERTICES_PER_CIRCLE].position = Vector3(RADIUS_OUTER * x, RADIUS_OUTER * y,  HALF_DEPTH);
	}

	for (int i = 0; i < VERTICES_PER_CIRCLE; i++) {
		// Quad 1
		gizmo.indices.push_back((i)     % VERTICES_PER_CIRCLE);
		gizmo.indices.push_back((i + 1) % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE);
		gizmo.indices.push_back((i)     % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE);

		gizmo.indices.push_back((i)     % VERTICES_PER_CIRCLE);
		gizmo.indices.push_back((i + 1) % VERTICES_PER_CIRCLE);
		gizmo.indices.push_back((i + 1) % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE);

		// Quad 2
		gizmo.indices.push_back((i)     % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE);
		gizmo.indices.push_back((i + 1) % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE * 2);
		gizmo.indices.push_back((i)     % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE * 2);

		gizmo.indices.push_back((i)     % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE);
		gizmo.indices.push_back((i + 1) % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE);
		gizmo.indices.push_back((i + 1) % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE * 2);

		// Quad 3
		gizmo.indices.push_back((i)     % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE * 2);
		gizmo.indices.push_back((i + 1) % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE * 3);
		gizmo.indices.push_back((i)     % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE * 3);

		gizmo.indices.push_back((i)     % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE * 2);
		gizmo.indices.push_back((i + 1) % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE * 2);
		gizmo.indices.push_back((i + 1) % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE * 3);

		// Quad 4
		gizmo.indices.push_back((i)     % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE * 3);
		gizmo.indices.push_back((i + 1) % VERTICES_PER_CIRCLE);
		gizmo.indices.push_back((i)     % VERTICES_PER_CIRCLE);

		gizmo.indices.push_back((i)     % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE * 3);
		gizmo.indices.push_back((i + 1) % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE * 3);
		gizmo.indices.push_back((i + 1) % VERTICES_PER_CIRCLE);
	}
	
	auto buffer_size_vertices = Util::vector_size_in_bytes(gizmo.vertices);
	auto buffer_size_indices  = Util::vector_size_in_bytes(gizmo.indices);

	gizmo.vertex_buffer = VulkanMemory::buffer_create(buffer_size_vertices, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	gizmo.index_buffer  = VulkanMemory::buffer_create(buffer_size_indices,  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VulkanMemory::buffer_copy_staged(gizmo.vertex_buffer, gizmo.vertices.data(), buffer_size_vertices);
	VulkanMemory::buffer_copy_staged(gizmo.index_buffer,  gizmo.indices .data(), buffer_size_indices);

	gizmo.calc_aabb();

	return gizmo;
}

static bool triangle_intersect(
	Vector3 const & triangle_position_0,
	Vector3 const & triangle_position_1,
	Vector3 const & triangle_position_2,
	Vector3 const & ray_origin,
	Vector3 const & ray_direction
) {
	Vector3 triangle_position_edge_1 = triangle_position_1 - triangle_position_0;
	Vector3 triangle_position_edge_2 = triangle_position_2 - triangle_position_0;

	Vector3 h = Vector3::cross(ray_direction, triangle_position_edge_2);
	float   a = Vector3::dot(triangle_position_edge_1, h);

	float f = 1.0f / a;

	Vector3 s = ray_origin - triangle_position_0;
	float   u = f * Vector3::dot(s, h);

	if (u >= 0.0f && u <= 1.0f) {
		Vector3 q = Vector3::cross(s, triangle_position_edge_1);
		float   v = f * Vector3::dot(ray_direction, q);

		if (v >= 0.0f && u + v <= 1.0f) {
			float t = f * Vector3::dot(triangle_position_edge_2, q);

			return t > 0.0f;
		}
	}

	return false;
}

bool Gizmo::intersects_mouse(Camera const & camera, int mouse_x, int mouse_y) {
	Vector3 ray_direction = camera.get_ray_direction(mouse_x, mouse_y);

	if (!aabb.intersects_ray(camera.position, ray_direction)) return false;

	for (int i = 0; i < indices.size(); i += 3) {
		bool intersects = triangle_intersect(
			vertices[indices[i    ]].position,
			vertices[indices[i + 1]].position,
			vertices[indices[i + 2]].position,
			camera.position,
			ray_direction
		);

		if (intersects) return true;
	}

	return false;
}
