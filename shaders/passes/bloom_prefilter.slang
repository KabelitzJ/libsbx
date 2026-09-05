// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <common.slang>
#include <descriptors.slang>

struct push_data {
  uint color_index;
  uint sampler_index;
  uint output_index;
  float threshold;
  float knee;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

struct compute_input {
  uint3 id : SV_DispatchThreadID;
}; // struct compute_input

// Karis-average anti-firefly weight -- downweights very bright taps so a single blown-out source
// pixel can't dominate the average (Brian Karis, "Physically Based Shading" course notes, SIGGRAPH 2013).
float karis_weight(float3 c) {
  return 1.0 / (1.0 + max(max(c.r, c.g), c.b));
}

[shader("compute")]
[numthreads(8, 8, 1)]
void compute_main(compute_input input) {
  uint width, height;
  storage_images[push.output_index].GetDimensions(width, height);

  if (input.id.x >= width || input.id.y >= height) {
    return;
  }

  uint color_width, color_height;
  textures[push.color_index].GetDimensions(color_width, color_height);

  float2 color_texel_size = 1.0 / float2(color_width, color_height);
  float2 uv = (float2(input.id.xy) + 0.5) / float2(width, height);

  // 4 bilinear taps over the source's 2x2 footprint for this half-res output texel -- each tap
  // already averages a 2x2 neighborhood, so this reads like a soft ~4x4 box without extra samples.
  float3 taps[4] = {
    textures[push.color_index].SampleLevel(samplers[push.sampler_index], uv + color_texel_size * float2(-0.5, -0.5), 0.0).rgb,
    textures[push.color_index].SampleLevel(samplers[push.sampler_index], uv + color_texel_size * float2( 0.5, -0.5), 0.0).rgb,
    textures[push.color_index].SampleLevel(samplers[push.sampler_index], uv + color_texel_size * float2(-0.5,  0.5), 0.0).rgb,
    textures[push.color_index].SampleLevel(samplers[push.sampler_index], uv + color_texel_size * float2( 0.5,  0.5), 0.0).rgb
  };

  float3 color = float3(0, 0, 0);
  float weight_sum = 0.0;

  for (uint i = 0u; i < 4u; ++i) {
    float w = karis_weight(taps[i]);
    color += taps[i] * w;
    weight_sum += w;
  }

  color /= max(weight_sum, 1e-4);

  // Soft-knee threshold curve (Jorge Jimenez, "Next Generation Post Processing in Call of Duty:
  // Advanced Warfare", SIGGRAPH 2014) -- fades bloom in smoothly around push.threshold instead of a
  // hard cutoff.
  float knee = max(push.knee, 1e-4);
  float3 curve = float3(push.threshold - knee, knee * 2.0, 0.25 / knee);

  float brightness = max(max(color.r, color.g), color.b);
  float soft = clamp(brightness - curve.x, 0.0, curve.y);
  soft = soft * soft * curve.z;

  float contribution = max(soft, brightness - push.threshold) / max(brightness, 1e-4);

  storage_images[push.output_index][input.id.xy] = float4(min(color * contribution, float3(firefly_clamp, firefly_clamp, firefly_clamp)), 1.0);
}
