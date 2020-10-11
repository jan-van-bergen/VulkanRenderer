#pragma once
#include <vector>

#include "Camera.h"
#include "Mesh.h"
#include "Lights.h"

struct Scene {
	Camera camera;
	
	std::vector<MeshInstance> meshes;
	
	std::vector<DirectionalLight> directional_lights;
	std::vector<PointLight>       point_lights;
	std::vector<SpotLight>        spot_lights;

	void init(int width, int height);
	void free();

	void update(float delta);
};
