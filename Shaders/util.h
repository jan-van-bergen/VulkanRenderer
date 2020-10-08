vec2 oct_wrap(vec2 v) {
	return vec2(
		(1.0f - abs(v.y)) * (v.x >= 0.0f ? +1.0f : -1.0f),
		(1.0f - abs(v.x)) * (v.y >= 0.0f ? +1.0f : -1.0f)
	);
}

// Based on: https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
vec2 pack_normal(vec3 n) {
	n /= (abs(n.x) + abs(n.y) + abs(n.z));
	n.xy = n.z >= 0.0f ? n.xy : oct_wrap(n.xy);
	return n.xy * 0.5f + 0.5f;
}

// Based on: https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
vec3 unpack_normal(vec2 f) {
	f = f * 2.0f - 1.0f;

	vec3 n = vec3(f.x, f.y, 1.0f - abs(f.x) - abs(f.y));

	float t = clamp(-n.z, 0.0f, 1.0f);
	n.x += n.x >= 0.0 ? -t : t;
	n.y += n.y >= 0.0 ? -t : t;

	return normalize(n);
}
