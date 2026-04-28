#ifndef LIBSBX_MATH_ALGORITHM_SLANG_
#define LIBSBX_MATH_ALGORITHM_SLANG_

namespace sbx::math {

float log10(float x) {
  return log(x) / log(10.0);
}

float satf(float x) {
  return clamp(x, 0.0, 1.0);
}

float2 satv(float2 x) {
 return clamp(x, float2(0.0), float2(1.0));
}

float max2(float2 v) {
  return max(v.x, v.y);
}

float max3(float3 v) {
  return max(max(v.r, v.g), v.b);
}

float mod(float x, float y) {
  return x - y * floor(x / y);
}

float2 mod(float2 x, float y) {
  return x - y * floor(x / y);
}

} // namespace sbx::math

#endif // LIBSBX_MATH_ALGORITHM_SLANG_
