#ifndef COMMON_DEPTH_SLANG_
#define COMMON_DEPTH_SLANG_

namespace sbx::common {

float linearize_depth(float depth, float near, float far) {
  float z_ndc = depth * 2.0 - 1.0;
  float z_eye = (2.0 * near * far) / ((far + near) - z_ndc * (far - near));

  return z_eye;
}

} // namespace sbx::common

#endif // COMMON_DEPTH_SLANG_
