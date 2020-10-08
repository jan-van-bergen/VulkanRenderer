struct Light {
	vec3 colour;
};

struct DirectionalLight {
	Light light;
	
	vec3 direction;
};

struct PointLight {
	Light light;
	
	vec3  position;
	float radius;
};

vec4 calc_light(Light light, vec3 position, vec3 direction, vec3 normal, vec3 camera_position) {
    vec4 result = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	
    float diffuse = dot(normal, -direction);
    
    if (diffuse > 0.0f) {
        result.rgb = light.colour * diffuse;

		// Calculate specular lighting (Blinn-Phong method)
        vec3 to_eye     = normalize(camera_position - position);
        vec3 half_angle = normalize(to_eye - direction);

        const float specular_power     = 32.0f;
        const float specular_intensity =  1.0f;

        float specular = dot(normal, half_angle);
        specular       = pow(specular, specular_power);

        result.rgb = light.colour * (diffuse + specular_intensity * specular);
    }

    return result;
}

vec4 calc_directional_light(DirectionalLight directional_light, vec3 position, vec3 normal, vec3 camera_position) {
    return calc_light(directional_light.light, position, directional_light.direction, normal, camera_position);
}

vec4 calc_point_light(PointLight point_light, vec3 position, vec3 normal, vec3 camera_position) {
    vec3 light_direction = position - point_light.position;
    
    vec4 colour = calc_light(point_light.light, position, normalize(light_direction), normal, camera_position);
    
    // Attenuation based on pointlight's range
	float distance_squared = dot(light_direction, light_direction);
	
    float attenuation = clamp(1.0f - distance_squared / (point_light.radius * point_light.radius), 0.0f, 1.0f);

    return colour * attenuation * attenuation;
}
