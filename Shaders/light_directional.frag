#version 450

layout(location = 0) in vec2 in_uv;

layout(location = 0) out vec4 out_colour;

layout(binding = 0) uniform sampler2D sampler_albedo;
layout(binding = 1) uniform sampler2D sampler_position;
layout(binding = 2) uniform sampler2D sampler_normal;

layout(binding = 3) uniform UniformBuffer {
	vec3 light_colour;

	vec3 light_direction;
	
	vec3 camera_position;
};

vec4 calc_light(vec3 world_position, vec3 direction, vec3 normal) {
    vec4 result = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	
    float diffuse = dot(normal, -direction);
    
    if (diffuse > 0.0f) {
        result.rgb = light_colour * diffuse;

		// Calculate specular lighting (Blinn-Phong method)
        vec3 to_eye     = normalize(camera_position - world_position);
        vec3 half_angle = normalize(to_eye - direction);

        const float specular_power     = 32.0f;
        const float specular_intensity =  1.0f;

        float specular = dot(normal, half_angle);
        specular       = pow(specular, specular_power);

        result.rgb = light_colour * (diffuse + specular_intensity * specular);
    }

    return result;
}

// Calculates lighting for a directional light
vec4 calc_directional_light(vec3 world_position, vec3 normal) {
    return calc_light(world_position, light_direction, normal);
}

void main() {
	vec3 albedo   = texture(sampler_albedo,   in_uv).xyz;
	vec3 position = texture(sampler_position, in_uv).xyz;
	vec3 normal   = texture(sampler_normal,   in_uv).xyz;

	out_colour = vec4(albedo, 1.0f) * calc_directional_light(position, normal);
}
