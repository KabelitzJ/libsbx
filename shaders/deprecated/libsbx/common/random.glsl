#ifndef COMMON_RANDOM_GLSL_
#define COMMON_RANDOM_GLSL_

vec2 random_2d(vec2 seed) {
  vec2 hash = vec2(
    dot(seed, vec2(127.1, 311.7)),
    dot(seed, vec2(269.5, 183.3))
  );
  
  return fract(sin(hash) * 43758.5453);
}

// Hash Functions for GPU Rendering, Jarzynski et al.
// http://www.jcgt.org/published/0009/03/02/
vec3 random_3d(uvec3 v) {
	v = v * 1664525u + 1013904223u;

	v.x += v.y * v.z;
	v.y += v.z * v.x;
	v.z += v.x * v.y;

	v ^= v >> 16u;

	v.x += v.y * v.z;
	v.y += v.z * v.x;
	v.z += v.x * v.y;

	return vec3(v) * (1.0 / float(0xffffffffu));
}

#endif // COMMON_RANDOM_GLSL_
