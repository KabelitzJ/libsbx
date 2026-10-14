// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <descriptors.slang>

struct vertex_input {
  uint id : SV_VertexID;
}; // struct vertex_input

struct vertex_output {
  [[vk::location(0)]] float2 uv : TEXCOORD;
  float4 sv_position : SV_Position;
}; // struct vertex_output

struct fragment_input {
  [[vk::location(0)]] float2 uv : TEXCOORD;
}; // struct fragment_input

struct fragment_output {
  float4 color : SV_Target0;
}; // struct fragment_output

// Mirrors fxaa_pass.cpp's fxaa_push.
struct push_data {
  uint source_index;
  uint sampler_index;
  float inverse_width;
  float inverse_height;
  uint enabled;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

// FXAA 3.11 quality defaults (Lottes).
static const float edge_threshold = 0.166;
static const float edge_threshold_min = 0.0833;
static const float subpixel_quality = 0.75;

static const int search_steps = 12;
static const float search_step_sizes[search_steps] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0 };

float3 sample_color(float2 uv) {
  return textures[push.source_index].SampleLevel(samplers[push.sampler_index], uv, 0.0).rgb;
}

// The source is sampled as linear (sRGB view), so take the square root to get a perceptual luma.
float luma(float3 color) {
  return sqrt(dot(color, float3(0.299, 0.587, 0.114)));
}

float luma_at(float2 uv, float2 offset, float2 texel) {
  return luma(sample_color(uv + offset * texel));
}

[shader("vertex")]
vertex_output vertex_main(vertex_input input) {
  float2 uv = float2((input.id << 1) & 2, input.id & 2);

  vertex_output output;

  output.uv = uv;
  output.sv_position = float4(uv * 2.0 - 1.0, 0.0, 1.0);

  return output;
}

[shader("fragment")]
fragment_output fragment_main(fragment_input input) {
  fragment_output output;

  float2 uv = input.uv;
  float2 texel = float2(push.inverse_width, push.inverse_height);

  float3 center_color = sample_color(uv);
  output.color = float4(center_color, 1.0);

  if (push.enabled == 0u) {
    return output;
  }

  // "up" is -y in uv space; the edge tests are symmetric, so only consistency matters.
  float luma_center = luma(center_color);
  float luma_up = luma_at(uv, float2(0.0, -1.0), texel);
  float luma_down = luma_at(uv, float2(0.0, 1.0), texel);
  float luma_left = luma_at(uv, float2(-1.0, 0.0), texel);
  float luma_right = luma_at(uv, float2(1.0, 0.0), texel);

  float luma_min = min(luma_center, min(min(luma_up, luma_down), min(luma_left, luma_right)));
  float luma_max = max(luma_center, max(max(luma_up, luma_down), max(luma_left, luma_right)));
  float luma_range = luma_max - luma_min;

  if (luma_range < max(edge_threshold_min, luma_max * edge_threshold)) {
    return output;
  }

  float luma_up_left = luma_at(uv, float2(-1.0, -1.0), texel);
  float luma_up_right = luma_at(uv, float2(1.0, -1.0), texel);
  float luma_down_left = luma_at(uv, float2(-1.0, 1.0), texel);
  float luma_down_right = luma_at(uv, float2(1.0, 1.0), texel);

  float luma_up_down = luma_up + luma_down;
  float luma_left_right = luma_left + luma_right;
  float luma_left_corners = luma_up_left + luma_down_left;
  float luma_right_corners = luma_up_right + luma_down_right;
  float luma_up_corners = luma_up_left + luma_up_right;
  float luma_down_corners = luma_down_left + luma_down_right;

  float edge_horizontal = abs(-2.0 * luma_left + luma_left_corners) + abs(-2.0 * luma_center + luma_up_down) * 2.0 + abs(-2.0 * luma_right + luma_right_corners);
  float edge_vertical = abs(-2.0 * luma_up + luma_up_corners) + abs(-2.0 * luma_center + luma_left_right) * 2.0 + abs(-2.0 * luma_down + luma_down_corners);
  bool is_horizontal = edge_horizontal >= edge_vertical;

  // The two neighbours across the edge: negative and positive along the step axis.
  float luma_negative = is_horizontal ? luma_up : luma_left;
  float luma_positive = is_horizontal ? luma_down : luma_right;
  float gradient_negative = abs(luma_negative - luma_center);
  float gradient_positive = abs(luma_positive - luma_center);
  bool is_negative_steepest = gradient_negative >= gradient_positive;
  float gradient_scaled = 0.25 * max(gradient_negative, gradient_positive);

  float step_length = is_horizontal ? texel.y : texel.x;
  float luma_local_average;

  if (is_negative_steepest) {
    step_length = -step_length;
    luma_local_average = 0.5 * (luma_negative + luma_center);
  } else {
    luma_local_average = 0.5 * (luma_positive + luma_center);
  }

  // Walk along the edge, half a texel onto it, in both directions until its luma changes enough.
  float2 edge_uv = uv;

  if (is_horizontal) {
    edge_uv.y += step_length * 0.5;
  } else {
    edge_uv.x += step_length * 0.5;
  }

  float2 along = is_horizontal ? float2(texel.x, 0.0) : float2(0.0, texel.y);

  float2 uv_negative = edge_uv - along * search_step_sizes[0];
  float2 uv_positive = edge_uv + along * search_step_sizes[0];
  float luma_end_negative = luma(sample_color(uv_negative)) - luma_local_average;
  float luma_end_positive = luma(sample_color(uv_positive)) - luma_local_average;
  bool reached_negative = abs(luma_end_negative) >= gradient_scaled;
  bool reached_positive = abs(luma_end_positive) >= gradient_scaled;

  for (int i = 1; i < search_steps && !(reached_negative && reached_positive); ++i) {
    if (!reached_negative) {
      uv_negative -= along * search_step_sizes[i];
      luma_end_negative = luma(sample_color(uv_negative)) - luma_local_average;
      reached_negative = abs(luma_end_negative) >= gradient_scaled;
    }

    if (!reached_positive) {
      uv_positive += along * search_step_sizes[i];
      luma_end_positive = luma(sample_color(uv_positive)) - luma_local_average;
      reached_positive = abs(luma_end_positive) >= gradient_scaled;
    }
  }

  float distance_negative = is_horizontal ? (uv.x - uv_negative.x) : (uv.y - uv_negative.y);
  float distance_positive = is_horizontal ? (uv_positive.x - uv.x) : (uv_positive.y - uv.y);
  bool is_negative_closer = distance_negative < distance_positive;
  float distance_final = min(distance_negative, distance_positive);
  float edge_length = distance_negative + distance_positive;

  // Only blend when the nearer end's luma moves away from the centre's side of the edge.
  bool is_luma_center_smaller = luma_center < luma_local_average;
  bool correct_variation = ((is_negative_closer ? luma_end_negative : luma_end_positive) < 0.0) != is_luma_center_smaller;
  float pixel_offset = correct_variation ? (0.5 - distance_final / edge_length) : 0.0;

  // Sub-pixel aliasing: how far the centre stands out from its 3x3 neighbourhood.
  float luma_average = (1.0 / 12.0) * (2.0 * (luma_up_down + luma_left_right) + luma_left_corners + luma_right_corners);
  float subpixel_a = saturate(abs(luma_average - luma_center) / luma_range);
  float subpixel_b = (-2.0 * subpixel_a + 3.0) * subpixel_a * subpixel_a;
  float subpixel_offset = subpixel_b * subpixel_b * subpixel_quality;

  float final_offset = max(pixel_offset, subpixel_offset);

  float2 final_uv = uv;

  if (is_horizontal) {
    final_uv.y += final_offset * step_length;
  } else {
    final_uv.x += final_offset * step_length;
  }

  output.color = float4(sample_color(final_uv), 1.0);

  return output;
}
