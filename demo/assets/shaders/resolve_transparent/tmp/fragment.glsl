#version 460 core

#include <libsbx/common/constants.glsl>
#include <libsbx/common/lighting.glsl>

layout(location = 0) in vec2 in_uv;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform uniform_scene {
  vec3 camera_position;
  vec3 light_direction;
  vec4 light_color;
  mat4 light_space;
  uint point_light_count;
} scene;

layout(set = 0, binding = 1, std430) readonly buffer buffer_point_lights {
  point_light data[];
} point_lights;

layout(set = 0, binding = 2) uniform sampler2D accum_image;
layout(set = 0, binding = 3) uniform sampler2D revealage_image; 

// calculate floating point numbers equality accurately
bool is_approximately_equal(float a, float b) {
  return abs(a - b) <= (abs(a) < abs(b) ? abs(b) : abs(a)) * EPSILON;
}

float max3(vec3 v) {
  return max(max(v.x, v.y), v.z);
}

void main() {
  // fragment coordination
  ivec2 coords = ivec2(gl_FragCoord.xy);

  // fragment revealage
  float revealage = texelFetch(revealage_image, coords, 0).r;

  // save the blending and color texture fetch cost if there is not a transparent fragment
  if (is_approximately_equal(revealage, 1.0f)) {
    clip(-1);
  }

  // fragment color
  vec4 accumulation = texelFetch(accum_image, coords, 0);

  // suppress overflow
  if (isinf(max3(abs(accumulation.rgb)))) {
    accumulation.rgb = vec3(accumulation.a);
  }

  // prevent floating point precision bug
  vec3 average = accumulation.rgb / max(accumulation.a, EPSILON);

  // blend pixels
  out_color = vec4(average, 1.0f - revealage);
}
