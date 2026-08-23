#ifndef LIBSBX_MATH_AABB_GLSL_
#define LIBSBX_MATH_AABB_GLSL_

struct aabb {
  float values[6]; // min_x, min_y, min_z, max_x, max_y, max_z
}; // struct aabb

aabb aabb_create(vec3 min, vec3 max) {
  aabb result;

  result.values[0] = min.x;
  result.values[1] = min.y;
  result.values[2] = min.z;
  result.values[3] = max.x;
  result.values[4] = max.y;
  result.values[5] = max.z;

  return result;
}

vec3 aabb_get_min(aabb box) {
  return vec3(box.values[0], box.values[1], box.values[2]);
}

vec3 aabb_get_max(aabb box) {
  return vec3(box.values[3], box.values[4], box.values[5]);
}

void aabb_set_min(aabb box, vec3 min) {
  box.values[0] = min.x;
  box.values[1] = min.y;
  box.values[2] = min.z;
}

void aabb_set_max(aabb box, vec3 max) {
  box.values[3] = max.x;
  box.values[4] = max.y;
  box.values[5] = max.z;
}

#endif // LIBSBX_MATH_AABB_GLSL_
