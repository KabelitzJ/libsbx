#ifndef COMMON_BRDF_SLANG_
#define COMMON_BRDF_SLANG_

#include <libsbx/math/constants.slang>

namespace sbx::common {

static const float3 default_f0 = float3(0.04, 0.04, 0.04);

float3 compute_f90(float3 f0, float specular_strength) {
  float reflectance = max(max(f0.r, f0.g), f0.b);

  return float3(saturate(reflectance * 50.0) * specular_strength);
}

float3 fresnel_schlick(float cos_theta, float3 f0, float3 f90) {
  return f0 + (f90 - f0) * pow(saturate(1.0 - cos_theta), 5.0);
}

float3 fresnel_schlick_roughness(float cos_theta, float3 f0, float roughness) {
  float3 f90 = max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), f0);

  return f0 + (f90 - f0) * pow(saturate(1.0 - cos_theta), 5.0);
}

float distribution_ggx(float3 n, float3 h, float alpha) {
  float a2 = alpha * alpha;

  float ndh  = max(dot(n, h), 0.0);
  float ndh2 = ndh * ndh;

  float denom = ndh2 * (a2 - 1.0) + 1.0;

  return a2 / (sbx::math::pi * denom * denom);
}

float v_smith_ggx_correlated(float ndv, float ndl, float alpha) {
  float a2 = alpha * alpha;
  float ggx_v = ndl * sqrt(ndv * ndv * (1.0 - a2) + a2);
  float ggx_l = ndv * sqrt(ndl * ndl * (1.0 - a2) + a2);

  return 0.5 / max(ggx_v + ggx_l, 1e-5);
}

} // namespace sbx::common

#endif // COMMON_BRDF_SLANG_