// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef SHADOWS_CSM_SLANG_
#define SHADOWS_CSM_SLANG_

#include <frame_data.slang>
#include <descriptors.slang>
#include <math.slang>

[[vk::constant_id(0)]]
const uint shadow_pcf_quality = 0u;

static const float2 poisson_disk_low[4] = {
  float2( 0.97484398,  0.75648379),
  float2(-0.81409955,  0.91437590),
  float2(-0.81544232, -0.87912464),
  float2( 0.94558609, -0.76890725)
};

static const float2 poisson_disk_medium[8] = {
  float2( 0.79197514,  0.19090188),
  float2( 0.19984126,  0.78641367),
  float2(-0.24188840,  0.99706507),
  float2(-0.91588581,  0.45771432),
  float2(-0.94201624, -0.39906216),
  float2(-0.81544232, -0.87912464),
  float2(-0.094184101, -0.92938870),
  float2( 0.94558609, -0.76890725)
};

static const float2 poisson_disk_high[16] = {
  float2(-0.94201624, -0.39906216),
  float2( 0.94558609, -0.76890725),
  float2(-0.094184101, -0.92938870),
  float2( 0.34495938,  0.29387760),
  float2(-0.91588581,  0.45771432),
  float2(-0.81544232, -0.87912464),
  float2(-0.38277543,  0.27676845),
  float2( 0.97484398,  0.75648379),
  float2( 0.44323325, -0.97511554),
  float2( 0.53742981, -0.47373420),
  float2(-0.26496911, -0.41893023),
  float2( 0.79197514,  0.19090188),
  float2(-0.24188840,  0.99706507),
  float2(-0.81409955,  0.91437590),
  float2( 0.19984126,  0.78641367),
  float2( 0.14383161, -0.14100790)
};

// Cascades cross-fade over the outer 10% of each sphere's usable radius; shadows fade out over the
// last 10% of the shadow distance.
static const float cascade_blend_threshold = 0.9;

// PCF radius bounds in texels: a far cascade can't resolve an edge sharper than about one texel,
// and a near cascade's radius is capped so the fixed Poisson tap count doesn't spread into noise.
static const float shadow_min_filter_texels = 1.0;
static const float shadow_max_filter_texels = 5.0;

struct cascade_select {
  uint cascade;
  float blend_factor; // Weight of cascade + 1; only non-zero where that cascade covers the point too.
  bool valid;
}; // struct cascade_select

// Interleaved gradient noise (Jimenez 2014) from the pixel's screen position: every 3x3 pixel block
// covers [0, 1) evenly, so the per-pixel PCF rotation and contact-shadow ray offset read as a fine,
// uniform dither instead of white-noise clumps.
float interleaved_gradient_noise(float2 pixel) {
  return frac(52.9829189 * frac(dot(pixel, float2(0.06711056, 0.00583715))));
}

float2x2 shadow_rotation_2d(float angle) {
  float s = sin(angle);
  float c = cos(angle);

  return float2x2(c, -s, s, c);
}

float2 shadow_ndc_to_uv(float2 ndc_xy) {
  return ndc_xy * 0.5 + 0.5;
}

// World-space distance a lookup can reach past the shaded point, at the largest possible PCSS
// radius: the receiver normal offset (normal_bias * (1 + radius) texels), the PCF footprint
// (radius + 1 bilinear texel), and one texel of texel-snapping shift of the map relative to the sphere.
float cascade_usable_radius(frame_data frame, uint cascade) {
  float margin_texels = 1.0 + (1.0 + shadow_max_filter_texels) * (1.0 + frame.shadow_normal_bias);

  return frame.cascade_spheres[cascade].w - margin_texels * frame.cascade_texel_world_size[cascade];
}

float cascade_sphere_distance(frame_data frame, uint cascade, float3 world_position) {
  return length(world_position - frame.cascade_spheres[cascade].xyz);
}

// Picks the finest cascade whose shadow map fully covers the lookup around world_position (sphere
// test, as in Unity's split-sphere selection). Near that sphere's edge it blends into the next
// cascade, but only if the next one covers the point as well -- otherwise it would read outside
// that map and blend in a fully-lit value.
cascade_select select_cascade(frame_data frame, float3 world_position) {
  cascade_select result;
  result.cascade = 0u;
  result.blend_factor = 0.0;
  result.valid = false;

  for (uint cascade = 0u; cascade < shadow_cascade_count; ++cascade) {
    float usable = cascade_usable_radius(frame, cascade);
    float distance = cascade_sphere_distance(frame, cascade, world_position);

    if (distance < usable) {
      result.cascade = cascade;
      result.valid = true;

      if (cascade + 1u < shadow_cascade_count && cascade_sphere_distance(frame, cascade + 1u, world_position) < cascade_usable_radius(frame, cascade + 1u)) {
        float blend_start = usable * cascade_blend_threshold;
        result.blend_factor = saturate((distance - blend_start) / (usable - blend_start));
      }

      return result;
    }
  }

  return result;
}

uint shadow_sample_count() {
  return shadow_pcf_quality == 0 ? 4u : (shadow_pcf_quality == 1 ? 8u : 16u);
}

float2 shadow_poisson_sample(uint i) {
  if (shadow_pcf_quality == 0) {
    return poisson_disk_low[i];
  }

  if (shadow_pcf_quality == 1) {
    return poisson_disk_medium[i];
  }

  return poisson_disk_high[i];
}

struct shadow_receiver {
  float2 uv;
  float depth;
  bool valid;
}; // struct shadow_receiver

// Projects world_position into the cascade after the receiver normal offset for a filter of
// radius_texels: a surface tilted by theta changes light-space depth by texel * tan(theta) per
// texel, and the lookup reaches (1 + radius) texels from the center (radius for the Poisson taps,
// 1 for SampleCmp's bilinear 2x2). Offsetting by d along the normal buys d / cos(theta) of depth
// margin, so d = (1 + radius) * texel * sin(theta) covers any flat receiver exactly at normal_bias 1.
shadow_receiver project_shadow_receiver(frame_data frame, uint cascade, float3 world_position, float3 world_normal, float sin_theta, float radius_texels) {
  float normal_offset = frame.shadow_normal_bias * (1.0 + radius_texels) * frame.cascade_texel_world_size[cascade] * sin_theta;

  float4 clip = mul(frame.light_view_projections[cascade], float4(world_position + world_normal * normal_offset, 1.0));
  float3 ndc = clip.xyz / clip.w;

  shadow_receiver result;
  result.uv = shadow_ndc_to_uv(ndc.xy);
  result.depth = ndc.z;
  result.valid = all(result.uv >= 0.0) && all(result.uv <= 1.0) && result.depth >= 0.0 && result.depth <= 1.0;

  return result;
}

// PCSS (Fernando 2005) for a directional light, whose angular size makes the penumbra grow with
// the distance between receiver and blocker: radius = distance * tan(angular_diameter / 2).
// 1. Blocker search: average the depth of everything closer to the light than the receiver, within
//    the widest penumbra possible here (a blocker at the cascade's near plane).
// 2. Penumbra estimate from the receiver-to-average-blocker distance. Cascade depth is linear
//    (orthographic, 2 * sphere radius deep), so depth differences convert straight to world units.
//    Depth-clamped casters sit at depth 0, so a blocker beyond the near plane reads as closer than
//    it is and gets a slightly sharper penumbra than reality.
// 3. PCF with that radius. Every radius is clamped to what the cascade can resolve (>= 1 texel) and
//    what the fixed tap count covers without noise (<= shadow_max_filter_texels).
float sample_cascade_pcf(frame_data frame, uint cascade, float3 world_position, float3 world_normal, float noise) {
  // Neighboring pixels can land in different cascades, so the descriptor index isn't uniform.
  Texture2D shadow_map = textures[NonUniformResourceIndex(frame.shadow_map_indices[cascade])];

  uint width, height;
  shadow_map.GetDimensions(width, height);

  float2 map_size = float2(width, height);
  float2 texel_size = 1.0 / map_size;
  int2 max_texel = int2(width, height) - 1;

  float texel_world = frame.cascade_texel_world_size[cascade];
  float depth_range = 2.0 * frame.cascade_spheres[cascade].w;
  float depth_bias = frame.cascade_depth_bias_per_texel[cascade] * frame.shadow_depth_bias;

  float3 light_direction = normalize(-frame.lights[0].direction.xyz);
  float ndl = dot(world_normal, light_direction);
  float sin_theta = sqrt(saturate(1.0 - ndl * ndl));

  float2x2 rotation = shadow_rotation_2d(noise * two_pi);
  uint sample_count = shadow_sample_count();

  shadow_receiver center = project_shadow_receiver(frame, cascade, world_position, world_normal, sin_theta, shadow_min_filter_texels);

  if (!center.valid) {
    return 1.0;
  }

  float search_radius = clamp(center.depth * depth_range * frame.shadow_penumbra_scale / texel_world, shadow_min_filter_texels, shadow_max_filter_texels);

  shadow_receiver search = project_shadow_receiver(frame, cascade, world_position, world_normal, sin_theta, search_radius);

  if (!search.valid) {
    return 1.0;
  }

  float search_reference = search.depth - depth_bias;
  float blocker_depth_sum = 0.0;
  float blocker_count = 0.0;

  for (uint i = 0u; i < sample_count; ++i) {
    float2 sample_uv = search.uv + mul(rotation, shadow_poisson_sample(i)) * texel_size * search_radius;
    int2 texel = clamp(int2(sample_uv * map_size), int2(0, 0), max_texel);
    float sample_depth = shadow_map.Load(int3(texel, 0)).r;

    if (sample_depth < search_reference) {
      blocker_depth_sum += sample_depth;
      blocker_count += 1.0;
    }
  }

  // The PCF radius below never exceeds search_radius, so its footprint lies inside the searched area.
  if (blocker_count == 0.0) {
    return 1.0;
  }

  float blocker_distance = (search.depth - blocker_depth_sum / blocker_count) * depth_range;
  float radius = clamp(blocker_distance * frame.shadow_penumbra_scale / texel_world, shadow_min_filter_texels, search_radius);

  shadow_receiver receiver = project_shadow_receiver(frame, cascade, world_position, world_normal, sin_theta, radius);

  if (!receiver.valid) {
    return 1.0;
  }

  float reference = receiver.depth - depth_bias;

  SamplerComparisonState shadow_sampler = comparison_samplers[frame.shadow_sampler_index];

  float shadow = 0.0;

  for (uint i = 0u; i < sample_count; ++i) {
    float2 offset = mul(rotation, shadow_poisson_sample(i));
    shadow += shadow_map.SampleCmpLevelZero(shadow_sampler, receiver.uv + offset * texel_size * radius, reference);
  }

  return shadow / float(sample_count);
}

float shadow_distance_fade(frame_data frame, float3 world_position) {
  float view_depth = -mul(frame.view, float4(world_position, 1.0)).z;
  float fade_start = frame.cascade_splits.w * cascade_blend_threshold;

  return saturate((view_depth - fade_start) / (frame.cascade_splits.w - fade_start));
}

float calculate_shadow(frame_data frame, float3 world_position, float3 world_normal, float noise) {
  cascade_select info = select_cascade(frame, world_position);

  if (!info.valid) {
    return 1.0;
  }

  float shadow = sample_cascade_pcf(frame, info.cascade, world_position, world_normal, noise);

  if (info.blend_factor > 0.0) {
    float next = sample_cascade_pcf(frame, info.cascade + 1u, world_position, world_normal, noise);

    shadow = lerp(shadow, next, info.blend_factor);
  }

  return lerp(shadow, 1.0, shadow_distance_fade(frame, world_position));
}

float3 shadow_cascade_debug_tint(frame_data frame, float3 world_position) {
  static const float3 cascade_colors[shadow_cascade_count] = {
    float3(1.0, 0.25, 0.25),
    float3(0.25, 1.0, 0.25),
    float3(0.25, 0.25, 1.0),
    float3(1.0, 1.0, 0.25)
  };

  cascade_select info = select_cascade(frame, world_position);

  if (!info.valid) {
    return float3(1.0, 1.0, 1.0);
  }

  float3 color = lerp(cascade_colors[info.cascade], cascade_colors[min(info.cascade + 1u, shadow_cascade_count - 1u)], info.blend_factor);

  return lerp(color, float3(1.0, 1.0, 1.0), shadow_distance_fade(frame, world_position));
}

#endif // SHADOWS_CSM_SLANG_
