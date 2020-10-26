#include "Scene.h"

Scene::Scene(int width, int height) : camera(DEG_TO_RAD(110.0f), width, height), asset_manager(*this) {
	Material * material_diffuse = materials.emplace_back(std::make_unique<Material>(0.9f, 0.0f)).get();

	animated_meshes.emplace_back(*this, "Cowboy",   asset_manager.load_animated_mesh("Data/Cowboy2.fbx"), material_diffuse);
	animated_meshes.emplace_back(*this, "XNA Dude", asset_manager.load_animated_mesh("Data/xnadude.fbx"), material_diffuse);
	animated_meshes.emplace_back(*this, "Arm",      asset_manager.load_animated_mesh("Data/test.fbx"),    material_diffuse);

	animated_meshes[0].loop = false;

	animated_meshes[0].animation_speed = 2.0f;
	animated_meshes[1].animation_speed = 15.0f;
	animated_meshes[2].animation_speed = 200.0f;

	animated_meshes[0].play_animation("Armature|Run");
	animated_meshes[1].play_animation(0);

	animated_meshes[0].transform.rotation = Quaternion::axis_angle(Vector3(1.0f, 0.0f, 0.0f), DEG_TO_RAD(-90.0f));
	animated_meshes[1].transform.scale = 0.1f;

	meshes.emplace_back("Monkey", asset_manager.load_mesh("Data/Monkey.obj"),        material_diffuse).transform.position = Vector3(  0.0f, -10.0f, 0.0f);
	meshes.emplace_back("Cube 1", asset_manager.load_mesh("Data/Cube.obj"),          material_diffuse).transform.position = Vector3( 10.0f,   0.0f, 0.0f);
	meshes.emplace_back("Cube 2", asset_manager.load_mesh("Data/Cube.obj"),          material_diffuse).transform.position = Vector3(-10.0f,   0.0f, 0.0f);
	meshes.emplace_back("Sponza", asset_manager.load_mesh("Data/Sponza/sponza.obj"), material_diffuse).transform.position = Vector3(  0.0f,  -7.5f, 0.0f);

	directional_lights.push_back({ Vector3(1.0f),
		Quaternion::axis_angle(Vector3(0.0f, 0.0f, 1.0f), std::tan(1.0f / 10.0f)) *
		Quaternion::axis_angle(Vector3(1.0f, 0.0f, 0.0f), DEG_TO_RAD(-90.0f))
	});

	point_lights.push_back({ Vector3(10.0f, 0.0f,  0.0f), Vector3(-6.0f, 0.0f, 0.0f), 16.0f });
	point_lights.push_back({ Vector3( 0.0f, 0.0f, 10.0f), Vector3(+6.0f, 0.0f, 0.0f), 16.0f });

	constexpr float point_lights_width  = 150.0f;
	constexpr float point_lights_height =  60.0f;

	auto rand_float = [](float min = 0.0f, float max = 1.0f) { return min + (max - min) * rand() / float(RAND_MAX); };

	for (int i = 0; i < 500; i++) {
		point_lights.push_back({ 
			5.0f + 10.0f * Vector3(rand_float(), rand_float(), rand_float()),
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

	if (animated_meshes.size() > 0) {
		animated_meshes[0].transform.position.x += delta;
		animated_meshes[0].transform.rotation = Quaternion::axis_angle(Vector3(0.0f, 1.0f, 0.0f), delta) * animated_meshes[0].transform.rotation;
	}

	if (meshes.size() > 0) meshes[0].transform.scale = 5.0f + std::sin(time);
	if (meshes.size() > 1) meshes[1].transform.rotation = Quaternion::axis_angle(Vector3(0.0f, 1.0f, 0.0f), delta) * meshes[1].transform.rotation;
	if (meshes.size() > 2) meshes[2].transform.rotation = Quaternion::axis_angle(Vector3(0.0f, 1.0f, 0.0f), delta) * meshes[2].transform.rotation;

	if (directional_lights.size() > 0) directional_lights[0].orientation = Quaternion::axis_angle(Vector3(0.0f, 1.0f, 0.0f), 0.2f * delta) * directional_lights[0].orientation;

	if (spot_lights.size() > 0) spot_lights[0].direction = Quaternion::axis_angle(Vector3(0.0f, 1.0f, 0.0f), 0.5f * delta) * spot_lights[0].direction;
	if (spot_lights.size() > 1) spot_lights[1].direction = Quaternion::axis_angle(Vector3(0.0f, 1.0f, 0.0f),       -delta) * spot_lights[1].direction;

	for (auto & animated_mesh : animated_meshes) {
		animated_mesh.update(delta);
		animated_mesh.transform.update();
	}

	for (auto & mesh : meshes) {
		mesh.transform.update();
	}
}
