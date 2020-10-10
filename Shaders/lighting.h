struct Light {
	vec3 colour;
};

struct DirectionalLight {
	Light base;
	
	vec3 direction;
};

struct PointLight {
	Light base;
	
	vec3  position;
	float one_over_radius_squared;
};

struct SpotLight {
	PointLight base;
	
	vec3 direction;
	
	float cutoff_inner;
	float cutoff_outer;
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
    return calc_light(directional_light.base, position, directional_light.direction, normal, camera_position);
}

vec4 calc_point_light(PointLight point_light, vec3 position, vec3 normal, vec3 camera_position) {
    vec3 light_direction = position - point_light.position;
    
    vec4 colour = calc_light(point_light.base, position, normalize(light_direction), normal, camera_position);
    
    // Attenuation based on pointlight's range
	float distance_squared = dot(light_direction, light_direction);
	
    float attenuation = clamp(1.0f - distance_squared * point_light.one_over_radius_squared, 0.0f, 1.0f);

    return colour * attenuation * attenuation;
}

vec4 calc_spot_light(SpotLight spot_light, vec3 position, vec3 normal, vec3 camera_position) {
	vec3 light_direction = position - spot_light.base.position;
	
	float spot_factor = dot(normalize(light_direction), spot_light.direction);
	
	if (spot_factor <= 0.0f) return vec4(0.0f);
	
	float radial_falloff = clamp((spot_factor - spot_light.cutoff_outer) / (spot_light.cutoff_inner - spot_light.cutoff_outer), 0.0f, 1.0f);
	
	return radial_falloff * calc_point_light(spot_light.base, position, normal, camera_position);
}
