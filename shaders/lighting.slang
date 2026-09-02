// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIGHTING_SLANG_
#define LIGHTING_SLANG_

#include <frame_data.slang>
#include <descriptors.slang>

#include <clusters/cluster_data.slang>
#include <shadows/csm.slang>

struct lighting_input {
  float3 position;
  float3 normal;
  float2 uv;
  float4 tangent;
  float4 sv_position;
  bool is_front_face;
  frame_data frame_data;
  uint material_index;
  uint sampler_index;
  uint clamp_sampler_index;
}; // struct lighting_input

struct lighting_output {
  float3 color;
  float alpha;
}; // struct lighting_output

float distribution_ggx(float3 n, float3 h, float roughness) {
  float a = roughness * roughness;
  float a2 = a * a;
  float ndh = max(dot(n, h), 0.0);
  float d = ndh * ndh * (a2 - 1.0) + 1.0;

  return a2 / (pi * d * d);
}

float geometry_schlick_ggx(float ndv, float roughness) {
  float r = roughness + 1.0;
  float k = (r * r) / 8.0;

  return ndv / (ndv * (1.0 - k) + k);
}

float geometry_smith(float3 n, float3 v, float3 l, float roughness) {
  return geometry_schlick_ggx(max(dot(n, v), 0.0), roughness) * geometry_schlick_ggx(max(dot(n, l), 0.0), roughness);
}

float3 fresnel_schlick(float cos_theta, float3 f0) {
  return f0 + (1.0 - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

float3 brdf(float3 n, float3 v, float3 l, float3 albedo, float3 f0, float roughness, float metallic) {
  float3 h = normalize(v + l);
  float ndf = distribution_ggx(n, h, roughness);
  float g = geometry_smith(n, v, l, roughness);
  float3 f = fresnel_schlick(max(dot(h, v), 0.0), f0);

  float3 specular = (ndf * g * f) / max(4.0 * max(dot(n, v), 0.0) * max(dot(n, l), 0.0), 0.0001);
  float3 kd = (1.0 - f) * (1.0 - metallic);

  return (kd * albedo / pi + specular) * max(dot(n, l), 0.0);
}

lighting_output evaluate_lighting<let ReceivesShadows : bool>(lighting_input input) {
  material_data material = input.frame_data.materials[input.material_index];
  SamplerState sampler = samplers[input.sampler_index];

  float4 base = material.base_color_factor * textures[material.albedo_index].Sample(sampler, input.uv);

  apply_alpha_cutout(material, input.uv, input.sampler_index);

  float3 albedo = base.rgb;

  float4 mr = textures[material.metallic_roughness_index].Sample(sampler, input.uv);
  float metallic = mr.b * material.metallic_factor;
  float roughness = mr.g * material.roughness_factor;

  // occlusionTexture.strength: lerp(1, sampled_ao, strength) so strength=0 leaves AO fully unapplied.
  float ao = lerp(1.0, textures[material.occlusion_index].Sample(sampler, input.uv).r, material.occlusion_strength);
  float3 emissive = material.emissive_factor.rgb * material.emissive_strength * textures[material.emissive_index].Sample(sampler, input.uv).rgb;

  float3 n = normalize(input.normal);

  // Double-sided materials disable backface culling (opaque_pass.cpp) but keep the same
  // interpolated (front-facing) normal — flip it for the backface so it still points toward the
  // viewer. Single-sided materials are backface-culled, so is_front_face is always true there.
  if (!input.is_front_face) {
    n = -n;
  }

  float3 t = normalize(input.tangent.xyz);

  // Gram-Schmidt re-orthogonalize after interpolation
  t = normalize(t - dot(t, n) * n);

  // handedness
  float3 b = cross(n, t) * input.tangent.w;

  float3 tangent_normal = textures[material.normal_index].Sample(sampler, input.uv).rgb * 2.0 - 1.0;
  tangent_normal.xy *= material.normal_scale;
  n = normalize(tangent_normal.x * t + tangent_normal.y * b + tangent_normal.z * n);

  float3 v = normalize(input.frame_data.camera_position.xyz - input.position);

  // KHR_materials_ior: F0 = ((ior-1)/(ior+1))^2; default ior 1.5 reproduces the standard 0.04.
  float dielectric_f0 = pow((material.ior - 1.0) / max(material.ior + 1.0, 0.0001), 2.0);
  float3 f0 = lerp(float3(dielectric_f0, dielectric_f0, dielectric_f0), albedo, metallic);
  float3 lo = float3(0.0, 0.0, 0.0);

  float sun_shadow = 1.0;

  for (uint i = 0u; i < input.frame_data.directional_light_count; ++i) {
    light_data light = input.frame_data.lights[i];

    float3 l = normalize(-light.direction.xyz);
    float3 radiance = light.color.rgb * light.color.a;

    if (ReceivesShadows && i == 0u && input.frame_data.shadow_enabled != 0u && (material.flags & material_flags::receives_shadow) != 0u) {
      sun_shadow = calculate_shadow(input.frame_data, input.position, n, input.clamp_sampler_index);

      radiance *= sun_shadow;
    }

    lo += brdf(n, v, l, albedo, f0, roughness, metallic) * radiance;
  }

  float view_z = mul(input.frame_data.view, float4(input.position, 1.0)).z;
  float depth_slice = log2(max(-view_z, 0.0001)) * input.frame_data.cluster_scale + input.frame_data.cluster_bias;
  uint slice = uint(clamp(depth_slice, 0.0, float(cluster_dimensions.z - 1u)));

  uint2 tile = uint2(input.sv_position.xy / input.frame_data.cluster_tile_size);
  tile = min(tile, uint2(cluster_dimensions.x - 1u, cluster_dimensions.y - 1u));

  uint cluster_id = tile.x + cluster_dimensions.x * (tile.y + cluster_dimensions.y * slice);
  range range = input.frame_data.cluster_ranges[cluster_id];

  for (uint c = 0u; c < range.count; ++c) {
    light_data light = input.frame_data.lights[input.frame_data.cluster_light_indices[range.offset + c]];

    float3 to_light = light.position.xyz - input.position;
    float distance = length(to_light);
    float3 l = to_light / max(distance, 0.0001);

    float range_falloff = light.position.w;
    float falloff = saturate(1.0 - pow(distance / max(range_falloff, 0.0001), 4.0));
    float attenuation = (falloff * falloff) / max(distance * distance, 0.0001);

    if (light.type == light_type::spot) {
      float cos_angle = dot(-l, normalize(light.direction.xyz));

      attenuation *= saturate((cos_angle - light.outer_cos) / max(light.inner_cos - light.outer_cos, 0.0001));
    }

    float3 radiance = light.color.rgb * light.color.a * attenuation;

    lo += brdf(n, v, l, albedo, f0, roughness, metallic) * radiance;
  }

  float ndv = max(dot(n, v), 0.0);
  float3 fr = fresnel_schlick(ndv, f0);
  float3 kd = (1.0 - fr) * (1.0 - metallic);

  float3 irradiance = (input.frame_data.irradiance_index != 0xFFFFFFFF) ? cubes[input.frame_data.irradiance_index].Sample(sampler, n).rgb : float3(0.03);
  float3 diffuse = irradiance * albedo / pi;

  float3 specular = float3(0.0, 0.0, 0.0);

  if (input.frame_data.prefiltered_mip_count > 0u && input.frame_data.brdf_lut_index != 0xFFFFFFFF) {
    float3 reflection = reflect(-v, n);

    float level = roughness * float(input.frame_data.prefiltered_mip_count - 1u);

    float3 prefiltered = cubes[input.frame_data.prefiltered_index].SampleLevel(sampler, reflection, level).rgb;

    SamplerState clamp_sampler = samplers[input.clamp_sampler_index];
    float2 brdf = textures[input.frame_data.brdf_lut_index].Sample(clamp_sampler, float2(ndv, roughness)).rg;

    specular = prefiltered * (fr * brdf.x + brdf.y);
  }

  float3 ambient = (kd * diffuse + specular) * ao * input.frame_data.ambient_intensity;

  static const float ambient_shadow_floor = 0.6;

  ambient *= lerp(ambient_shadow_floor, 1.0, sun_shadow);

  float3 color = ambient + lo + emissive;

  lighting_output output;

  output.color = color;
  output.alpha = base.a;

  return output;
}

#endif // LIGHTING_SLANG_
