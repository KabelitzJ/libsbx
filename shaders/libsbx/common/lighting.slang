#ifndef COMMON_LIGHTING_SLANG_
#define COMMON_LIGHTING_SLANG_

#include <libsbx/common/brdf.slang>
#include <libsbx/common/surface.slang>
#include <libsbx/math/constants.slang>

namespace sbx::common {

struct point_light {
  float4 position;  // xyz = position, w = radius
  float4 color;     // rgb = color, a = intensity
}; // struct point_light

struct directional_light {
  float4 direction; // xyz = direction (normalized), w = unused
  float4 color;     // rgb = color, a = intensity
}; // struct directional_light

float3 evaluate_direct_brdf(surface surface, float3 light_direction, float3 radiance) {
  float3 h = normalize(surface.view_direction + light_direction);
  float ndl = max(dot(surface.world_normal, light_direction), 0.0);

  float D = distribution_ggx(surface.world_normal, h, surface.alpha);
  float V = v_smith_ggx_correlated(surface.ndv, ndl, surface.alpha);

  float3 f90 = compute_f90(surface.f0, surface.specular_strength);
  float3 F = fresnel_schlick(max(dot(h, surface.view_direction), 0.0), surface.f0, f90);

  float3 specular = D * V * F;
  float3 kd = (1.0 - F) * (1.0 - surface.metallic);

  return (kd * surface.albedo / sbx::math::pi + specular) * radiance * ndl;
}

float3 evaluate_directional(surface surface, directional_light light) {
  float3 ld = normalize(-light.direction.xyz);
  float3 radiance = light.color.rgb * light.color.a;

  return evaluate_direct_brdf(surface, ld, radiance);
}

float3 evaluate_point(surface surface, point_light light) {
  float3 to_light = light.position.xyz - surface.world_position;
  float distance = max(length(to_light), 0.001);
  float3 ld = to_light / distance;

  float radius = light.position.w;
  float d_over_r = distance / radius;
  float d_over_r_4 = d_over_r * d_over_r * d_over_r * d_over_r;
  float clamped = saturate(1.0 - d_over_r_4);
  float attenuation = (clamped * clamped) / max(distance * distance, 1e-4);

  float3 radiance = light.color.rgb * light.color.a * attenuation;

  return evaluate_direct_brdf(surface, ld, radiance);
}

float3 evaluate_sun(surface surface, float3 sun_direction, float3 sun_color_intensity, float shadow) {
  float3 radiance = sun_color_intensity * shadow;
  return evaluate_direct_brdf(surface, sun_direction, radiance);
}

float3 evaluate_directional_array(surface surface, StructuredBuffer<directional_light> buffer, uint count) {
  float3 color = float3(0, 0, 0);

  for (uint i = 0u; i < count; ++i) {
    color += evaluate_directional(surface, buffer[i]);
  }

  return color;
}

float3 evaluate_point_array(surface surface, StructuredBuffer<point_light> buffer, uint count) {
  float3 color = float3(0, 0, 0);

  for (uint i = 0u; i < count; ++i) {
    color += evaluate_point(surface, buffer[i]);
  }

  return color;
}

} // namespace sbx::common

#endif // COMMON_LIGHTING_SLANG_
