#include "Gizmo.h"

#include "Util.h"

static constexpr int VERTICES_PER_CIRCLE = 12;

Gizmo Gizmo::generate_position() {
	constexpr float RADIUS_TAIL = 0.1f;
	constexpr float RADIUS_HEAD = 0.25f;
	constexpr float LENGTH_TAIL = 2.0f;
	constexpr float LENTGH_HEAD = 0.5f;

	// Generate Vertices
	std::vector<Vector3> vertices(3 * VERTICES_PER_CIRCLE + 2);

	vertices[0] = Vector3(0.0f, 0.0f, 0.0f);

	for (int i = 0; i < VERTICES_PER_CIRCLE; i++) {
		auto angle = TWO_PI * float(i) / float(VERTICES_PER_CIRCLE);
		auto x = std::cos(angle);
		auto y = std::sin(angle);

		vertices[i + 1]                           = Vector3(RADIUS_TAIL * x, RADIUS_TAIL * y, 0.0f);
		vertices[i + 1 + VERTICES_PER_CIRCLE]     = Vector3(RADIUS_TAIL * x, RADIUS_TAIL * y, -LENGTH_TAIL);
		vertices[i + 1 + VERTICES_PER_CIRCLE * 2] = Vector3(RADIUS_HEAD * x, RADIUS_HEAD * y, -LENGTH_TAIL);
	}

	vertices[1 + 3 * VERTICES_PER_CIRCLE] = Vector3(0.0f, 0.0f, -(LENGTH_TAIL + LENTGH_HEAD));

	// Generate Indices
	std::vector<int> indices;

	// Backside of tail
	for (int i = 1; i < VERTICES_PER_CIRCLE; i++) {
		indices.push_back(0);
		indices.push_back(i);
		indices.push_back(i + 1);
	}

	indices.push_back(0);
	indices.push_back(VERTICES_PER_CIRCLE);
	indices.push_back(1);

	// Edge loop that connects tail to Head
	for (int i = 1; i < VERTICES_PER_CIRCLE; i++) {
		indices.push_back(i);
		indices.push_back(i + VERTICES_PER_CIRCLE);
		indices.push_back(i + VERTICES_PER_CIRCLE + 1);

		indices.push_back(i);
		indices.push_back(i + VERTICES_PER_CIRCLE + 1);
		indices.push_back(i + 1);
	}

	indices.push_back(VERTICES_PER_CIRCLE);
	indices.push_back(VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE);
	indices.push_back(VERTICES_PER_CIRCLE + 1);

	indices.push_back(VERTICES_PER_CIRCLE);
	indices.push_back(VERTICES_PER_CIRCLE + 1);
	indices.push_back(1);

	// Outer loop of head
	for (int i = 1 + VERTICES_PER_CIRCLE; i < 2 * VERTICES_PER_CIRCLE; i++) {
		indices.push_back(i);
		indices.push_back(i + VERTICES_PER_CIRCLE);
		indices.push_back(i + VERTICES_PER_CIRCLE + 1);

		indices.push_back(i);
		indices.push_back(i + VERTICES_PER_CIRCLE + 1);
		indices.push_back(i + 1);
	}

	indices.push_back(VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE);
	indices.push_back(VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE);
	indices.push_back(VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE + 1);

	indices.push_back(VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE);
	indices.push_back(VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE + 1);
	indices.push_back(VERTICES_PER_CIRCLE + 1);

	// Connect outer loop of head to tip of head
	auto last = vertices.size() - 1;

	for (int i = 1 + VERTICES_PER_CIRCLE * 2; i < 3 * VERTICES_PER_CIRCLE; i++) {
		indices.push_back(i);
		indices.push_back(last);
		indices.push_back(i + 1);
	}

	indices.push_back(3 * VERTICES_PER_CIRCLE);
	indices.push_back(last);
	indices.push_back(2 * VERTICES_PER_CIRCLE + 1);

	Gizmo gizmo;
	gizmo.index_count = indices.size();

	auto buffer_size_vertices = Util::vector_size_in_bytes(vertices);
	auto buffer_size_indices  = Util::vector_size_in_bytes(indices);

	gizmo.vertex_buffer = VulkanMemory::buffer_create(buffer_size_vertices, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	gizmo.index_buffer  = VulkanMemory::buffer_create(buffer_size_indices,  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VulkanMemory::buffer_copy_staged(gizmo.vertex_buffer, vertices.data(), buffer_size_vertices);
	VulkanMemory::buffer_copy_staged(gizmo.index_buffer,  indices .data(), buffer_size_indices);

	return gizmo;
}

Gizmo Gizmo::generate_rotation() {
	constexpr float RADIUS_INNER = 2.0f;
	constexpr float RADIUS_OUTER = 2.2f;
	constexpr float HALF_DEPTH   = 0.1f;

	// Generate Vertices
	std::vector<Vector3> vertices(4 * VERTICES_PER_CIRCLE);
	
	for (int i = 0; i < VERTICES_PER_CIRCLE; i++) {
		auto angle = TWO_PI * float(i) / float(VERTICES_PER_CIRCLE);
		auto x = std::cos(angle);
		auto y = std::sin(angle);

		vertices[i]                           = Vector3(RADIUS_INNER * x, RADIUS_INNER * y,  HALF_DEPTH);
		vertices[i + 1 * VERTICES_PER_CIRCLE] = Vector3(RADIUS_INNER * x, RADIUS_INNER * y, -HALF_DEPTH);
		vertices[i + 2 * VERTICES_PER_CIRCLE] = Vector3(RADIUS_OUTER * x, RADIUS_OUTER * y, -HALF_DEPTH);
		vertices[i + 3 * VERTICES_PER_CIRCLE] = Vector3(RADIUS_OUTER * x, RADIUS_OUTER * y,  HALF_DEPTH);
	}

	// Generate Indices
	std::vector<int> indices;
	indices.reserve(4 * 2 * 3 * VERTICES_PER_CIRCLE); // 4 sides, each with 2 triangles

	for (int i = 0; i < VERTICES_PER_CIRCLE; i++) {
		// Quad 1
		indices.push_back((i)     % VERTICES_PER_CIRCLE);
		indices.push_back((i + 1) % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE);
		indices.push_back((i)     % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE);

		indices.push_back((i)     % VERTICES_PER_CIRCLE);
		indices.push_back((i + 1) % VERTICES_PER_CIRCLE);
		indices.push_back((i + 1) % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE);

		// Quad 2
		indices.push_back((i)     % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE);
		indices.push_back((i + 1) % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE * 2);
		indices.push_back((i)     % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE * 2);

		indices.push_back((i)     % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE);
		indices.push_back((i + 1) % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE);
		indices.push_back((i + 1) % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE * 2);

		// Quad 3
		indices.push_back((i)     % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE * 2);
		indices.push_back((i + 1) % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE * 3);
		indices.push_back((i)     % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE * 3);

		indices.push_back((i)     % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE * 2);
		indices.push_back((i + 1) % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE * 2);
		indices.push_back((i + 1) % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE * 3);

		// Quad 4
		indices.push_back((i)     % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE * 3);
		indices.push_back((i + 1) % VERTICES_PER_CIRCLE);
		indices.push_back((i)     % VERTICES_PER_CIRCLE);

		indices.push_back((i)     % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE * 3);
		indices.push_back((i + 1) % VERTICES_PER_CIRCLE + VERTICES_PER_CIRCLE * 3);
		indices.push_back((i + 1) % VERTICES_PER_CIRCLE);
	}
	
	Gizmo gizmo;
	gizmo.index_count = indices.size();

	auto buffer_size_vertices = Util::vector_size_in_bytes(vertices);
	auto buffer_size_indices  = Util::vector_size_in_bytes(indices);

	gizmo.vertex_buffer = VulkanMemory::buffer_create(buffer_size_vertices, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	gizmo.index_buffer  = VulkanMemory::buffer_create(buffer_size_indices,  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VulkanMemory::buffer_copy_staged(gizmo.vertex_buffer, vertices.data(), buffer_size_vertices);
	VulkanMemory::buffer_copy_staged(gizmo.index_buffer,  indices .data(), buffer_size_indices);

	return gizmo;
}
