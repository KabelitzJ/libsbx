#ifndef MATH_SLANG_
#define MATH_SLANG_

static const float pi = 3.14159265359;
static const float two_pi = 6.28318530718;

static const float firefly_clamp = 30.0f;

float2 rotate(float2 position, float rotation) {
  float c = cos(rotation);
  float s = sin(rotation);

  return float2(
    position.x * c - position.y * s,
    position.x * s + position.y * c
  );
}

#endif // MATH_SLANG_
