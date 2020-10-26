#pragma once
#include <vector>

#include "Camera.h"
#include "Mesh.h"
#include "Lights.h"

#include "AssetManager.h"

struct Scene {
	AssetManager asset_manager;

	Camera camera;
	
	std::vector<std::unique_ptr<Material>> materials;

	std::vector<MeshInstance>         meshes;
	std::vector<AnimatedMeshInstance> animated_meshes;

	std::vector<DirectionalLight> directional_lights;
	std::vector<PointLight>       point_lights;
	std::vector<SpotLight>        spot_lights;

	Scene(int width, int height);

	void update(float delta);
};
