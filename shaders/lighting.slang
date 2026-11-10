// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIGHTING_SLANG_
#define LIGHTING_SLANG_

#include <frame_data.slang>
#include <descriptors.slang>

#include <clusters/cluster_data.slang>
#include <shadows/csm.slang>
#include <shadows/contact.slang>
#include <shadows/local.slang>

struct lighting_input {
  float3 position;
  float3 normal;
  float2 uv;
  float4 tangent;
  float4 color;
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

struct surface {
  float3 albedo;
  float3 n;
  float3 v;
  float3 f0;
  float roughness;
  float metallic;
}; // struct surface

struct lit_surface {
  float3 albedo;
  float3 tangent_space_normal;
  float metallic;
  float roughness;
  float3 emissive;
  float ao;
  float alpha;
  float dielectric_f0;
  bool receives_shadow;
}; // struct lit_surface

interface lighting_model {
  static float3 direct(surface s, float3 l, float3 radiance);
  static float3 extra(surface s);
}; // interface lighting_model

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

float3 fresnel_schlick_roughness(float cos_theta, float3 f0, float roughness) {
  float3 f90 = max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), f0);
  return f0 + (f90 - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
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

struct pbr_lighting_model : lighting_model {
  static float3 direct(surface s, float3 l, float3 radiance) {
    return brdf(s.n, s.v, l, s.albedo, s.f0, s.roughness, s.metallic) * radiance;
  }

  static float3 extra(surface s) {
    return float3(0.0, 0.0, 0.0);
  }
}; // struct pbr_lighting_model

// The cluster input.sv_position and view-space depth view_z fall in.
cluster_range find_cluster(lighting_input input, float view_z) {
  float depth_slice = log2(max(-view_z, 0.0001)) * input.frame_data.cluster_scale + input.frame_data.cluster_bias;
  uint slice = uint(clamp(depth_slice, 0.0, float(cluster_dimensions.z - 1u)));

  uint2 tile = uint2(input.sv_position.xy / input.frame_data.cluster_tile_size);
  tile = min(tile, uint2(cluster_dimensions.x - 1u, cluster_dimensions.y - 1u));

  uint cluster_id = tile.x + cluster_dimensions.x * (tile.y + cluster_dimensions.y * slice);

  return input.frame_data.cluster_ranges[cluster_id];
}

// Projects the cluster's decals onto the surface in sort order: each blends its albedo over `albedo` and bends the world normal `n` toward its normal map.
// position_dx/position_dy are the world position's screen derivatives, taken by the caller in uniform control flow, since decals are sampled inside per-pixel branches.
void apply_decals(lighting_input input, cluster_range cluster, float3 vertex_normal, float3 position_dx, float3 position_dy, inout float3 albedo, inout float3 n) {
  SamplerState sampler = samplers[input.clamp_sampler_index];

  for (uint c = 0u; c < cluster.decal_count; ++c) {
    decal_data decal = input.frame_data.decals[input.frame_data.cluster_indices[cluster.offset + cluster.light_count + c]];

    float3 local = mul(decal.world_to_decal, float4(input.position, 1.0)).xyz;

    if (any(abs(local) > 0.5)) {
      continue;
    }

    // How squarely the surface faces back at the projector, and how far from the box's near and far faces it sits.
    float facing = dot(vertex_normal, normalize(decal.axis_z.xyz));
    float angle_weight = saturate((facing - decal.axis_z.w) / max(decal.axis_y.w - decal.axis_z.w, 0.0001));
    float edge_weight = saturate((0.5 - abs(local.z)) / max(decal.edge_fade * 0.5, 0.0001));
    float weight = decal.color.a * angle_weight * edge_weight;

    if (weight <= 0.0) {
      continue;
    }

    // The texture's +Y is the decal's +Y, so v runs down it.
    float2 uv = float2(local.x + 0.5, 0.5 - local.y);
    float2 uv_dx = mul(decal.world_to_decal, float4(position_dx, 0.0)).xy * float2(1.0, -1.0);
    float2 uv_dy = mul(decal.world_to_decal, float4(position_dy, 0.0)).xy * float2(1.0, -1.0);

    float coverage = weight;

    if (decal.albedo_index != 0xFFFFFFFFu) {
      float4 texel = textures[decal.albedo_index].SampleGrad(sampler, uv, uv_dx, uv_dy);
      coverage *= texel.a;
      albedo = lerp(albedo, texel.rgb * decal.color.rgb, coverage);
    } else {
      albedo = lerp(albedo, decal.color.rgb, coverage);
    }

    if (decal.normal_index == 0xFFFFFFFFu || decal.axis_x.w <= 0.0) {
      continue;
    }

    // The decal's tangent frame laid onto the surface: its X projected into the surface plane, and the bitangent from that.
    float3 t = decal.axis_x.xyz - n * dot(decal.axis_x.xyz, n);

    if (dot(t, t) < 1e-8) {
      continue;
    }

    t = normalize(t);
    float3 b = cross(n, t);

    float3 decal_normal = textures[decal.normal_index].SampleGrad(sampler, uv, uv_dx, uv_dy).rgb * 2.0 - 1.0;
    float3 bent = normalize(decal_normal.x * t + decal_normal.y * b + decal_normal.z * n);

    n = normalize(lerp(n, bent, saturate(coverage * decal.axis_x.w)));
  }
}

// What the cluster's reflection probes contribute to a pixel's ambient light: their irradiance and prefiltered radiance, already weighted, and how much of the ambient they cover (the global environment fills the rest).
struct probe_lighting {
  float3 irradiance;
  float3 prefiltered;
  float weight;
}; // struct probe_lighting

// Blends the first two of the cluster's probes (most important first) whose box holds the pixel, each fading in over its blend distance from the box's faces.
// With box projection, the reflection ray is traced to the box's wall and the capture sampled toward that point, so nearby walls line up with the reflections of them.
probe_lighting sample_reflection_probes(lighting_input input, cluster_range cluster, float3 n, float3 reflection, float roughness) {
  probe_lighting result;
  result.irradiance = float3(0.0, 0.0, 0.0);
  result.prefiltered = float3(0.0, 0.0, 0.0);
  result.weight = 0.0;

  SamplerState sampler = samplers[input.sampler_index];

  uint used = 0u;

  for (uint c = 0u; c < cluster.probe_count && used < 2u; ++c) {
    probe_data probe = input.frame_data.probes[input.frame_data.cluster_indices[cluster.offset + cluster.light_count + cluster.decal_count + c]];

    float3 local = mul(probe.world_to_box, float4(input.position, 1.0)).xyz;
    float3 inside = probe.extents.xyz - abs(local);
    float depth = min(inside.x, min(inside.y, inside.z));

    if (depth <= 0.0) {
      continue;
    }

    float weight = probe.center.w > 0.0 ? saturate(depth / probe.center.w) : 1.0;
    float share = weight * (1.0 - result.weight);

    float3 direction = reflection;

    if (probe.axis_y.w != 0.0) {
      float3 local_direction = mul(probe.world_to_box, float4(reflection, 0.0)).xyz;

      // Keeps axis-parallel rays from dividing by zero; they never reach those walls anyway.
      float3 safe_direction = float3(abs(local_direction.x) < 1e-5 ? 1e-5 : local_direction.x, abs(local_direction.y) < 1e-5 ? 1e-5 : local_direction.y, abs(local_direction.z) < 1e-5 ? 1e-5 : local_direction.z);

      float3 to_positive = (probe.extents.xyz - local) / safe_direction;
      float3 to_negative = (-probe.extents.xyz - local) / safe_direction;
      float3 exits = max(to_positive, to_negative);
      float distance = min(exits.x, min(exits.y, exits.z));

      float3 hit = local + local_direction * distance;

      direction = hit.x * normalize(probe.axis_x.xyz) + hit.y * normalize(probe.axis_y.xyz) + hit.z * normalize(probe.axis_z.xyz);
    }

    float level = roughness * float(max(probe.prefiltered_mip_count, 1u) - 1u);
    float intensity = probe.axis_x.w;

    result.irradiance += share * intensity * cubes[probe.irradiance_index].SampleLevel(sampler, n, 0.0).rgb;
    result.prefiltered += share * intensity * cubes[probe.prefiltered_index].SampleLevel(sampler, direction, level).rgb;
    result.weight += share;

    ++used;
  }

  return result;
}

// ReceivesShadows is true only for the opaque pass's policy, which is also the only pass decals reach: transparents receive neither.
lighting_output evaluate_lit_surface<let ReceivesShadows : bool>(lighting_input input, lit_surface s) {
  float3 n = normalize(input.normal);

  if (!input.is_front_face) {
    n = -n;
  }

  float3 vertex_normal = n;

  // Taken here, in uniform control flow: the contact shadow call below sits behind per-pixel branches.
  float view_z = mul(input.frame_data.view, float4(input.position, 1.0)).z;
  float depth_slope = fwidth(view_z);
  float3 position_dx = ddx(input.position);
  float3 position_dy = ddy(input.position);

  cluster_range cluster = find_cluster(input, view_z);

  float3 t = normalize(input.tangent.xyz);

  // Gram-Schmidt re-orthogonalize after interpolation
  t = normalize(t - dot(t, n) * n);

  // handedness
  float3 b = cross(n, t) * input.tangent.w;

  float3 tangent_normal = s.tangent_space_normal;
  n = normalize(tangent_normal.x * t + tangent_normal.y * b + tangent_normal.z * n);

  if (ReceivesShadows) {
    apply_decals(input, cluster, vertex_normal, position_dx, position_dy, s.albedo, n);
  }

  float3 v = normalize(input.frame_data.camera_position.xyz - input.position);

  float3 f0 = lerp(float3(s.dielectric_f0, s.dielectric_f0, s.dielectric_f0), s.albedo, s.metallic);

  surface surf;
  surf.albedo = s.albedo;
  surf.n = n;
  surf.v = v;
  surf.f0 = f0;
  surf.roughness = s.roughness;
  surf.metallic = s.metallic;

  float3 lo = float3(0.0, 0.0, 0.0);

  float sun_shadow = 1.0;

  for (uint i = 0u; i < input.frame_data.directional_light_count; ++i) {
    light_data light = input.frame_data.lights[i];

    float3 l = normalize(-light.direction.xyz);
    float3 radiance = light.color.rgb * light.color.a;

    if (ReceivesShadows && i == 0u && input.frame_data.shadow_enabled != 0u && s.receives_shadow) {
      float noise = interleaved_gradient_noise(input.sv_position.xy);

      sun_shadow = calculate_shadow(input.frame_data, input.position, vertex_normal, noise);

      if (input.frame_data.contact_shadow_length > 0.0 && sun_shadow > 0.0 && dot(vertex_normal, l) > 0.0) {
        sun_shadow = min(sun_shadow, calculate_contact_shadow(input.frame_data, input.position, l, noise, depth_slope));
      }

      radiance *= sun_shadow;
    }

    lo += pbr_lighting_model::direct(surf, l, radiance);
  }

  for (uint c = 0u; c < cluster.light_count; ++c) {
    light_data light = input.frame_data.lights[input.frame_data.cluster_indices[cluster.offset + c]];

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

    if (ReceivesShadows && s.receives_shadow && light.shadow_index != 0xFFFFFFFFu && attenuation > 0.0) {
      attenuation *= calculate_local_shadow(input.frame_data, light, input.position, vertex_normal, interleaved_gradient_noise(input.sv_position.xy));
    }

    float3 radiance = light.color.rgb * light.color.a * attenuation;

    lo += pbr_lighting_model::direct(surf, l, radiance);
  }

  lo += pbr_lighting_model::extra(surf);

  SamplerState sampler = samplers[input.sampler_index];

  float ndv = max(dot(n, v), 0.0);
  float3 fr = fresnel_schlick_roughness(ndv, f0, s.roughness);
  float3 kd = (1.0 - fr) * (1.0 - s.metallic);

  float3 reflection = reflect(-v, n);

  probe_lighting probes = sample_reflection_probes(input, cluster, n, reflection, s.roughness);

  float3 irradiance = (input.frame_data.irradiance_index != 0xFFFFFFFF) ? cubes[input.frame_data.irradiance_index].Sample(sampler, n).rgb : float3(0.03);
  irradiance = probes.irradiance + (1.0 - probes.weight) * irradiance;

  float3 diffuse = irradiance * s.albedo / pi;

  float3 specular = float3(0.0, 0.0, 0.0);

  bool has_global_specular = input.frame_data.prefiltered_mip_count > 0u;

  if (input.frame_data.brdf_lut_index != 0xFFFFFFFF && (has_global_specular || probes.weight > 0.0)) {
    float3 prefiltered = probes.prefiltered;

    if (has_global_specular) {
      float level = s.roughness * float(input.frame_data.prefiltered_mip_count - 1u);

      prefiltered += (1.0 - probes.weight) * cubes[input.frame_data.prefiltered_index].SampleLevel(sampler, reflection, level).rgb;
    }

    SamplerState clamp_sampler = samplers[input.clamp_sampler_index];
    // Explicit level: whether probes cover the pixel varies per pixel, so this can sit in divergent control flow.
    float2 brdf = textures[input.frame_data.brdf_lut_index].SampleLevel(clamp_sampler, float2(ndv, s.roughness), 0.0).rg;

    specular = prefiltered * (fr * brdf.x + brdf.y);
  }

  float3 ambient = (kd * diffuse + specular) * s.ao * input.frame_data.ambient_intensity;

  // Screen-space ambient occlusion (ambient_occlusion_pass): this pixel's, from the half-resolution
  // result. Ambient and environment light only -- direct light has its own shadows.
  if (input.frame_data.ambient_occlusion_index != 0xFFFFFFFFu) {
    float2 screen_uv = input.sv_position.xy / input.frame_data.render_target_size;
    ambient *= textures[input.frame_data.ambient_occlusion_index].SampleLevel(samplers[input.clamp_sampler_index], screen_uv, 0.0).r;
  }

  float3 color = ambient + lo + s.emissive;

  if (input.frame_data.shadow_cascade_debug != 0u && input.frame_data.shadow_enabled != 0u) {
    color *= shadow_cascade_debug_tint(input.frame_data, input.position);
  }

  lighting_output output;

  output.color = color;
  output.alpha = s.alpha;

  return output;
}

// The built-in pbr material's surface, from its fixed texture slots and factors -- what
// evaluate_lighting shades, and a starting point for a code shader to adjust
// (material/code_material.slang).
lit_surface builtin_lit_surface(lighting_input input) {
  material_data material = input.frame_data.materials[input.material_index];
  SamplerState sampler = samplers[input.sampler_index];
  float2 uv = apply_uv_transform(material, input.uv);

  float4 base = material.base_color_factor * input.color * textures[material.albedo_index].Sample(sampler, uv);

  float4 mr = textures[material.metallic_roughness_index].Sample(sampler, uv);

  float ao = lerp(1.0, textures[material.occlusion_index].Sample(sampler, uv).r, material.occlusion_strength);

  float3 tangent_normal = textures[material.normal_index].Sample(sampler, uv).rgb * 2.0 - 1.0;
  tangent_normal.xy *= material.normal_scale;

  lit_surface s;
  s.albedo = base.rgb;
  s.tangent_space_normal = tangent_normal;
  s.metallic = mr.b * material.metallic_factor;
  s.roughness = mr.g * material.roughness_factor;
  s.emissive = material.emissive_factor.rgb * material.emissive_strength * textures[material.emissive_index].Sample(sampler, uv).rgb;
  s.ao = ao;
  s.alpha = base.a;
  s.dielectric_f0 = pow((material.ior - 1.0) / max(material.ior + 1.0, 0.0001), 2.0);
  s.receives_shadow = (material.flags & material_flags::receives_shadow) != 0u;

  return s;
}

lighting_output evaluate_lighting<let ReceivesShadows : bool>(lighting_input input) {
  material_data material = input.frame_data.materials[input.material_index];

  apply_alpha_cutout(material, input.uv, input.sampler_index);

  return evaluate_lit_surface<ReceivesShadows>(input, builtin_lit_surface(input));
}

lighting_output evaluate_unlit(lighting_input input) {
  material_data material = input.frame_data.materials[input.material_index];
  SamplerState sampler = samplers[input.sampler_index];
  float2 uv = apply_uv_transform(material, input.uv);

  float4 base = material.base_color_factor * input.color * textures[material.albedo_index].Sample(sampler, uv);

  apply_alpha_cutout(material, input.uv, input.sampler_index);

  float3 emissive = material.emissive_factor.rgb * material.emissive_strength * textures[material.emissive_index].Sample(sampler, uv).rgb;

  lighting_output output;

  output.color = base.rgb + emissive;
  output.alpha = base.a;

  return output;
}

#endif // LIGHTING_SLANG_
