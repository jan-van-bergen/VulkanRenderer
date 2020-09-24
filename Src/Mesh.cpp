#include "Mesh.h"

#include <unordered_map>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader/tiny_obj_loader.h>

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
				vertex.texcoord = Vector2(attrib.texcoords[index.texcoord_index*2 + 0], 1.0f - attrib.texcoords[index.texcoord_index*2 + 1]);
				vertex.colour   = Vector3(1.0f);
			}

			mesh->indices.push_back(vertex_index);
		}
	}

	return mesh.get();
}

void Mesh::free() {
	cache.clear();
}
