// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef SHADOWS_CSM_SLANG_
#define SHADOWS_CSM_SLANG_

#include <frame_data.slang>
#include <descriptors.slang>
#include <math.slang>

[[vk::constant_id(0)]]
const uint shadow_pcf_quality = 2u;

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

static const float cascade_blend_threshold = 0.9;

struct cascade_select {
  uint cascade;
  uint next_cascade;
  float blend_factor;
  bool fade_out;
}; // struct cascade_select

float shadow_hash21(float2 seed) {
  return frac(sin(dot(seed, float2(12.9898, 78.233))) * 43758.5453);
}

float2x2 shadow_rotation_2d(float angle) {
  float s = sin(angle);
  float c = cos(angle);

  return float2x2(c, -s, s, c);
}

float2 shadow_ndc_to_uv(float2 ndc_xy) {
  return ndc_xy * 0.5 + 0.5;
}

cascade_select select_cascade(float view_depth, float4 splits) {
  cascade_select result;
  result.fade_out = false;

  if (view_depth < splits.x) {
    result.cascade = 0;
    result.next_cascade = 1;
    float blend_start = splits.x * cascade_blend_threshold;
    result.blend_factor = saturate((view_depth - blend_start) / (splits.x - blend_start));
  } else if (view_depth < splits.y) {
    result.cascade = 1;
    result.next_cascade = 2;
    float blend_start = splits.y * cascade_blend_threshold;
    result.blend_factor = saturate((view_depth - blend_start) / (splits.y - blend_start));
  } else if (view_depth < splits.z) {
    result.cascade = 2;
    result.next_cascade = 3;
    float blend_start = splits.z * cascade_blend_threshold;
    result.blend_factor = saturate((view_depth - blend_start) / (splits.z - blend_start));
  } else {
    result.cascade = 3;
    result.next_cascade = 3;
    result.fade_out = true;
    float fade_start = splits.w * cascade_blend_threshold;
    result.blend_factor = saturate((view_depth - fade_start) / (splits.w - fade_start));
  }

  return result;
}

float sample_cascade_pcf(frame_data frame, uint cascade, float3 world_position, float3 world_normal, uint sampler_index) {
  uint shadow_index = frame.shadow_map_indices[cascade];

  uint width, height;
  textures[shadow_index].GetDimensions(width, height);

  float2 texel_size = 1.0 / float2(width, height);

  float4 clip = mul(frame.light_view_projections[cascade], float4(world_position, 1.0));
  float3 ndc = clip.xyz / clip.w;
  float2 uv = shadow_ndc_to_uv(ndc.xy);

  if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
    return 1.0;
  }

  float depth = ndc.z;

  if (depth < 0.0 || depth > 1.0) {
    return 1.0;
  }

  float3 light_direction = normalize(-frame.lights[0].direction.xyz);
  float ndl = dot(world_normal, light_direction);

  static const float min_bias = 0.0005;
  static const float max_bias = 0.003;
  float bias = lerp(max_bias, min_bias, saturate(ndl));

  float resolution_scale = float(width) / 2048.0;
  float radius = (1.0 / resolution_scale) * (1.0 + float(cascade) * 0.3);

  float angle = shadow_hash21(uv) * two_pi;
  float2x2 rotation = shadow_rotation_2d(angle);

  SamplerState shadow_sampler = samplers[sampler_index];

  float shadow = 0.0;
  float sample_count;

  if (shadow_pcf_quality == 0) {
    sample_count = 4.0;

    [unroll]
    for (int i = 0; i < 4; ++i) {
      float2 offset = mul(rotation, poisson_disk_low[i]);
      float2 sample_uv = uv + offset * texel_size * radius;
      float sample_depth = textures[shadow_index].Sample(shadow_sampler, sample_uv).r;

      shadow += (depth - bias <= sample_depth) ? 1.0 : 0.0;
    }
  } else if (shadow_pcf_quality == 1) {
    sample_count = 8.0;

    [unroll]
    for (int i = 0; i < 8; ++i) {
      float2 offset = mul(rotation, poisson_disk_medium[i]);
      float2 sample_uv = uv + offset * texel_size * radius;
      float sample_depth = textures[shadow_index].Sample(shadow_sampler, sample_uv).r;

      shadow += (depth - bias <= sample_depth) ? 1.0 : 0.0;
    }
  } else {
    sample_count = 16.0;

    [unroll]
    for (int i = 0; i < 16; ++i) {
      float2 offset = mul(rotation, poisson_disk_high[i]);
      float2 sample_uv = uv + offset * texel_size * radius;
      float sample_depth = textures[shadow_index].Sample(shadow_sampler, sample_uv).r;

      shadow += (depth - bias <= sample_depth) ? 1.0 : 0.0;
    }
  }

  return shadow / sample_count;
}

float calculate_shadow(frame_data frame, float3 world_position, float3 world_normal, uint sampler_index) {
  float view_depth = -mul(frame.view, float4(world_position, 1.0)).z;
  cascade_select info = select_cascade(view_depth, frame.cascade_splits);

  float current = sample_cascade_pcf(frame, info.cascade, world_position, world_normal, sampler_index);

  if (info.fade_out) {
    return lerp(current, 1.0, info.blend_factor);
  }

  if (info.blend_factor > 0.0 && info.cascade < 3) {
    float next = sample_cascade_pcf(frame, info.next_cascade, world_position, world_normal, sampler_index);

    return lerp(current, next, info.blend_factor);
  }

  return current;
}

#endif // SHADOWS_CSM_SLANG_
