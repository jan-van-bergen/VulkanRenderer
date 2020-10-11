#include "Scene.h"

void Scene::init(int width, int height) {
	camera.init(DEG_TO_RAD(110.0f), width, height);

	meshes.push_back({ "Monkey", Mesh::load("Data/Monkey.obj") });
	meshes.push_back({ "Cube 1", Mesh::load("Data/Cube.obj"), Vector3( 10.0f, 0.0f, 0.0f) });
	meshes.push_back({ "Cube 2", Mesh::load("Data/Cube.obj"), Vector3(-10.0f, 0.0f, 0.0f) });
	meshes.push_back({ "Sponza", Mesh::load("Data/Sponza/sponza.obj"), Vector3(0.0f, -7.5f, 0.0f) });

	directional_lights.push_back({ Vector3(1.0f), Vector3::normalize(Vector3(1.0f, -1.0f, 0.0f)) });

	point_lights.push_back({ Vector3(1.0f, 0.0f, 0.0f), Vector3(-6.0f, 0.0f, 0.0f), 16.0f });
	point_lights.push_back({ Vector3(0.0f, 0.0f, 1.0f), Vector3(+6.0f, 0.0f, 0.0f), 16.0f });

	constexpr float point_lights_width  = 150.0f;
	constexpr float point_lights_height =  60.0f;

	auto rand_float = [](float min = 0.0f, float max = 1.0f) { return min + (max - min) * rand() / float(RAND_MAX); };

	for (int i = 0; i < 500; i++) {
		point_lights.push_back({ 
			10.0f * Vector3(rand_float(), rand_float(), rand_float()),
			Vector3(rand_float(-point_lights_width, point_lights_width), -7.0f, rand_float(-point_lights_height, point_lights_height)),
			rand_float(0.5f, 10.0f)
		});
	}

	spot_lights.push_back({ Vector3(10.0f,  0.0f, 10.0f), Vector3(-4.0f, -7.45f, 10.0f), 20.0f, Vector3(0.0f, 0.0f, 1.0f), DEG_TO_RAD(75.0f), DEG_TO_RAD(90.0f) });
	spot_lights.push_back({ Vector3( 0.0f, 10.0f,  0.0f), Vector3(+4.0f, -7.45f, 10.0f), 20.0f, Vector3(0.0f, 0.0f, 1.0f), DEG_TO_RAD(40.0f), DEG_TO_RAD(45.0f) });
}

void Scene::update(float delta) {
	camera.update(delta);

	static float time = 0.0f;
	time += delta;

	if (meshes.size() > 0) meshes[0].transform.scale = 5.0f + std::sin(time);
	if (meshes.size() > 1) meshes[1].transform.rotation = Quaternion::axis_angle(Vector3(0.0f, 1.0f, 0.0f), delta) * meshes[1].transform.rotation;
	if (meshes.size() > 2) meshes[2].transform.rotation = Quaternion::axis_angle(Vector3(0.0f, 1.0f, 0.0f), delta) * meshes[2].transform.rotation;

	if (directional_lights.size() > 0) directional_lights[0].direction = Quaternion::axis_angle(Vector3(0.0f, 0.0f, -1.0f), 0.2f * delta) * directional_lights[0].direction;
}