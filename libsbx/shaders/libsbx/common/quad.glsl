#ifndef COMMON_QUAD_GLSL_
#define COMMON_QUAD_GLSL_

struct vertex2d {
  vec2 position;
  vec2 uv;
}; // struct vertex2d

const vertex2d BASE_QUAD[6] = vertex2d[](
  vertex2d(vec2(-1.0, -1.0), vec2(0.0, 0.0)),
  vertex2d(vec2( 1.0, -1.0), vec2(1.0, 0.0)),
  vertex2d(vec2( 1.0,  1.0), vec2(1.0, 1.0)),
  vertex2d(vec2(-1.0, -1.0), vec2(0.0, 0.0)),
  vertex2d(vec2( 1.0,  1.0), vec2(1.0, 1.0)),
  vertex2d(vec2(-1.0,  1.0), vec2(0.0, 1.0))
);

#endif // COMMON_QUAD_GLSL_
