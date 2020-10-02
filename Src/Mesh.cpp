#include "Mesh.h"

#include <unordered_map>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader/tiny_obj_loader.h>

#include "VulkanContext.h"
#include "VulkanMemory.h"

#include "Util.h"

static std::unordered_map<std::string, std::unique_ptr<Mesh>> cache;

Mesh * Mesh::load(std::string const & filename) {
	std::unique_ptr<Mesh> & mesh = cache[filename];

	if (mesh != nullptr) return mesh.get();

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t>    shapes;
	std::vector<tinyobj::material_t> materials;

	std::string warning;
	std::string error;

	std::string path = filename.substr(0, filename.find_last_of("/\\"));

	bool success = tinyobj::LoadObj(&attrib, &shapes, &materials, &warning, &error, filename.c_str(), path.c_str());
	if (!success) {
		printf("ERROR: Unable to open obj file %s!\n", filename.c_str());
		abort();
	}

	mesh = std::make_unique<Mesh>();
	
	struct IndexHash {
		size_t operator()(tinyobj::index_t const & index) const {
			size_t hash = 17;
			hash = hash * 37 + std::hash<int>()(index.vertex_index);
			hash = hash * 37 + std::hash<int>()(index.texcoord_index);
			hash = hash * 37 + std::hash<int>()(index.normal_index);

			return hash;
		}
	};

	struct IndexCmp {
		bool operator()(tinyobj::index_t const & left, tinyobj::index_t const & right) const {
			return
				left.vertex_index   == right.vertex_index   &&
				left.texcoord_index == right.texcoord_index &&
				left.normal_index   == right.normal_index;
		}
	};

	std::unordered_map<tinyobj::index_t, int, IndexHash, IndexCmp> vertex_cache;

	for (auto const & shape : shapes) {
		for (size_t i = 0; i < shape.mesh.indices.size(); i++) {
			auto const & index = shape.mesh.indices[i];

			int vertex_index;

			// Check if we've seen this index (combination of v, vt, and vn indices) before
			if (vertex_cache.find(index) != vertex_cache.end()) {
				vertex_index = vertex_cache[index];
			} else {
				vertex_index = mesh->vertices.size();

				vertex_cache.insert(std::make_pair(index, vertex_index));

				auto & vertex = mesh->vertices.emplace_back();
				vertex.position = Vector3(&attrib.vertices[index.vertex_index*3]);
				vertex.texcoord = Vector2(attrib.texcoords[index.texcoord_index*2], 1.0f - attrib.texcoords[index.texcoord_index*2 + 1]);
				vertex.normal   = Vector3(&attrib.normals [index.normal_index*3]);
			}

			mesh->indices.push_back(vertex_index);
		}
	}

	// Upload Vertex and Index Buffer
	auto vertex_buffer_size = Util::vector_size_in_bytes(mesh->vertices);
	auto index_buffer_size  = Util::vector_size_in_bytes(mesh->indices);

	mesh->vertex_buffer = VulkanMemory::buffer_create(vertex_buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	mesh->index_buffer  = VulkanMemory::buffer_create(index_buffer_size,  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VulkanMemory::buffer_copy_staged(mesh->vertex_buffer, mesh->vertices.data(), vertex_buffer_size);
	VulkanMemory::buffer_copy_staged(mesh->index_buffer,  mesh->indices.data(),  index_buffer_size);

	return mesh.get();
}

void Mesh::free() {
	auto device = VulkanContext::get_device();

	for (auto & kvp : cache) {
		auto & mesh = kvp.second;

		VulkanMemory::buffer_free(mesh->vertex_buffer);
		VulkanMemory::buffer_free(mesh->index_buffer);
	}

	cache.clear();
}
