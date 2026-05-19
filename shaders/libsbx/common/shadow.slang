#ifndef COMMON_SHADOW_SLANG_
#define COMMON_SHADOW_SLANG_

#include <libsbx/common/scene_data.slang>
#include <libsbx/common/material.slang>
#include <libsbx/math/constants.slang>

namespace sbx::common {

static const float2 POISSON_DISK[16] = {
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

static const float CASCADE_BLEND_THRESHOLD = 0.9;

struct cascade_select {
  uint cascade;
  uint next_cascade;
  float blend_factor;
  bool fade_out;
}; // struct cascade_select

float _hash21(float2 seed) {
  return frac(sin(dot(seed, float2(12.9898, 78.233))) * 43758.5453);
}

float2x2 _rotation_2d(float angle) {
  float s = sin(angle);
  float c = cos(angle);
  return float2x2(c, -s, s, c);
}

float2 _ndc_to_uv(float2 ndc_xy) {
  return ndc_xy * 0.5 + 0.5;
}

cascade_select select_cascade(float view_depth, float4 splits) {
  cascade_select result;
  result.fade_out = false;

  if (view_depth < splits.x) {
    result.cascade = 0;
    result.next_cascade = 1;
    float blend_start = splits.x * CASCADE_BLEND_THRESHOLD;
    result.blend_factor = saturate((view_depth - blend_start) / (splits.x - blend_start));
  } else if (view_depth < splits.y) {
    result.cascade = 1;
    result.next_cascade = 2;
    float blend_start = splits.y * CASCADE_BLEND_THRESHOLD;
    result.blend_factor = saturate((view_depth - blend_start) / (splits.y - blend_start));
  } else if (view_depth < splits.z) {
    result.cascade = 2;
    result.next_cascade = 3;
    float blend_start = splits.z * CASCADE_BLEND_THRESHOLD;
    result.blend_factor = saturate((view_depth - blend_start) / (splits.z - blend_start));
  } else {
    result.cascade = 3;
    result.next_cascade = 3;
    result.fade_out = true;
    float fade_start = splits.w * CASCADE_BLEND_THRESHOLD;
    result.blend_factor = saturate((view_depth - fade_start) / (splits.w - fade_start));
  }

  return result;
}

float sample_cascade_pcf(Sampler2DArray<float> shadow_map, uint cascade, float4x4 light_space, float3 light_direction, float3 world_position, float3 world_normal) {
  uint2 dimensions;
  uint elements;
  shadow_map.GetDimensions(dimensions.x, dimensions.y, elements);

  float2 texel_size = 1.0 / float2(dimensions);
  float ndl = dot(world_normal, light_direction);

  // float normal_offset_scale = texel_size.x * 0.5 * (1.0 + cascade);
  // float3 offset_position = world_position + world_normal * normal_offset_scale * saturate(1.0 - ndl);

  float4 clip = mul(light_space, float4(world_position, 1.0));
  float3 ndc = clip.xyz / clip.w;
  float2 uv = _ndc_to_uv(ndc.xy);

  if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
    return 1.0;
  }

  float depth = ndc.z;

  if (depth < 0.0 || depth > 1.0) {
    return 1.0;
  }

  // float base_bias = max(0.002 * (1.0 - ndl), 0.0005);
  // float base_bias = 0.002 * (1.0 - ndl);
  float base_bias = 0.0005 * (1.0 - ndl);
  // float bias = base_bias * (1.0 + cascade * 0.5);
  float bias = base_bias;  // was: base_bias * (1.0 + cascade * 0.5)

  float resolution_scale = float(dimensions.x) / 2048.0;
  float radius = (1.0 / resolution_scale) * (1.0 + cascade * 0.3);

  float angle = _hash21(uv) * sbx::math::two_pi;
  float2x2 rotation = _rotation_2d(angle);

  float shadow = 0.0;

  [unroll]
  for (int i = 0; i < 16; ++i) {
    float2 offset = mul(rotation, POISSON_DISK[i]);
    float2 sample_uv = uv + offset * texel_size * radius;
    float sample_depth = shadow_map.Sample(float3(sample_uv, float(cascade)));

    shadow += (depth - bias <= sample_depth) ? 1.0 : 0.0;
  }

  return shadow / 16.0;
}

float calculate_shadow(Sampler2DArray<float> shadow_map, scene_data scene, float3 world_position, float3 world_normal, uint material_flags) {
  if (!has_feature(material_flags, material_feature::receive_shadow)) {
    return 1.0;
  }

  float view_depth = -mul(scene.view, float4(world_position, 1.0)).z;
  cascade_select info = select_cascade(view_depth, scene.cascade_splits);

  float3 light_direction = normalize(-scene.light_direction);

  float current = sample_cascade_pcf(shadow_map, info.cascade, scene.light_spaces[info.cascade], light_direction, world_position, world_normal);

  if (info.fade_out) {
    return lerp(current, 1.0, info.blend_factor);
  }

  if (info.blend_factor > 0.0 && info.cascade < 3) {
    float next = sample_cascade_pcf(shadow_map, info.next_cascade, scene.light_spaces[info.next_cascade], light_direction, world_position, world_normal);

    return lerp(current, next, info.blend_factor);
  }

  return current;
}

} // namespace sbx::common

#endif // COMMON_SHADOW_SLANG_
