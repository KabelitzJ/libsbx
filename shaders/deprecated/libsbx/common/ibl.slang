#ifndef COMMON_IBL_SLANG_
#define COMMON_IBL_SLANG_

#include <libsbx/common/brdf.slang>
#include <libsbx/common/surface.slang>
#include <libsbx/math/constants.slang>

namespace sbx::common {

static const float prefiltered_env_max_mip = 9.0;

float3 evaluate_ibl(surface surface, SamplerCube<float4> irradiance_map, SamplerCube<float4> prefiltered_map, Sampler2D<float4> brdf_lut, float ssao) {
  float3 r = normalize(reflect(-surface.view_direction, surface.world_normal));

  float3 diffuse_irradiance = irradiance_map.Sample(surface.world_normal).rgb;

  float lod = clamp(surface.roughness * prefiltered_env_max_mip, 0.0, prefiltered_env_max_mip);
  float3 prefiltered_color = prefiltered_map.SampleLevel(r, lod).rgb;

  float2 brdf_sample_point = clamp(float2(surface.ndv, surface.roughness), float2(0.0, 0.0), float2(1.0, 1.0));
  float2 f_ab = brdf_lut.Sample(brdf_sample_point).rg;

  float3 k_S = fresnel_schlick_roughness(surface.ndv, surface.f0, surface.roughness);

  float3 fss_ess = surface.specular_strength * k_S * f_ab.x + f_ab.y;

  float ems = 1.0 - (f_ab.x + f_ab.y);
  float3 f_avg = surface.specular_strength * (surface.f0 + (1.0 - surface.f0) / 21.0);
  float3 fms_ems = ems * fss_ess * f_avg / (1.0 - f_avg * ems);

  float3 diffuse_color = surface.albedo * (1.0 - surface.metallic);
  float3 k_D = diffuse_color * (1.0 - fss_ess + fms_ems);

  float3 diffuse_ibl = (fms_ems + k_D) * diffuse_irradiance / sbx::math::pi;
  float3 specular_ibl = surface.specular_strength * prefiltered_color * fss_ess;

  return (diffuse_ibl + specular_ibl) * surface.ao * ssao;
}

} // namespace sbx::common

#endif // COMMON_IBL_SLANG_