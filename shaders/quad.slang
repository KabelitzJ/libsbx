#ifndef QUAD_SLANG_
#define QUAD_SLANG_

struct quad {

  static const float2 vertices[4] = {
    float2(-1.0, -1.0),
    float2( 1.0, -1.0),
    float2( 1.0,  1.0),
    float2(-1.0,  1.0)
  };

  static const int indices[6] = { 
    0, 1, 2, 
    2, 3, 0 
  };

}; // struct quad

#endif // QUAD_SLANG_
