#ifndef COMMON_IBL_SLANG_
#define COMMON_IBL_SLANG_

#include <libsbx/common/brdf.slang>
#include <libsbx/common/surface.slang>
#include <libsbx/math/constants.slang>

namespace sbx::common {

static const float PREFILTERED_ENV_MAX_MIP = 9.0;

float3 evaluate_ibl(surface surface, SamplerCube<float4> irradiance_map, SamplerCube<float4> prefiltered_map, Sampler2D<float2> brdf_lut, float shadow, float ssao) {
  float3 r = normalize(reflect(-surface.view_direction, surface.world_normal));

  float3 diffuse_irradiance = irradiance_map.Sample(surface.world_normal).rgb;
  float3 diffuse_ibl = diffuse_irradiance * surface.albedo / sbx::math::pi;

  float lod = clamp(surface.roughness * PREFILTERED_ENV_MAX_MIP, 0.0, PREFILTERED_ENV_MAX_MIP);
  float3 prefiltered_color = prefiltered_map.SampleLevel(r, lod).rgb;

  float3 F_ibl = fresnel_schlick_roughness(surface.ndv, surface.f0, surface.specular_strength, surface.roughness);
  float3 kD_ibl = (1.0 - F_ibl) * (1.0 - surface.metallic);

  float2 brdf = brdf_lut.Sample(float2(surface.ndv, surface.roughness)).rg;
  float3 specular_ibl = prefiltered_color * (F_ibl * brdf.x + brdf.y);

  float shadow_occlusion = lerp(0.3, 1.0, shadow);

  return (kD_ibl * diffuse_ibl * shadow_occlusion + specular_ibl) * surface.ao * ssao;
}

} // namespace sbx::common

#endif // COMMON_IBL_SLANG_
