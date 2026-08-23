#ifndef COMMON_GENERATE_GLSL_
#define COMMON_GENERATE_GLSL_

struct vertex_data {
  vec2 position;
  vec2 uv;
}; // struct vertex_data

vertex_data generate_vertex_data(int index) {
  float uv_x = (index == 2) ? 2.0 : 0.0;
  float uv_y = (index == 1) ? 2.0 : 0.0;

  vec2 uv = vec2(uv_x, uv_y);

  vec2 position = uv * vec2(2.0, -2.0) + vec2(-1.0, 1.0);

  return vertex_data(position, uv);
}

#endif // COMMON_GENERATE_GLSL_
