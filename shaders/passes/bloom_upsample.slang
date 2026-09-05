// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <descriptors.slang>

struct push_data {
  uint small_index;
  uint big_index;
  uint sampler_index;
  uint output_index;
  float filter_radius;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

struct compute_input {
  uint3 id : SV_DispatchThreadID;
}; // struct compute_input

float3 sample_small(float2 uv) {
  return textures[push.small_index].SampleLevel(samplers[push.sampler_index], uv, 0.0).rgb;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void compute_main(compute_input input) {
  uint width, height;
  storage_images[push.output_index].GetDimensions(width, height);

  if (input.id.x >= width || input.id.y >= height) {
    return;
  }

  uint small_width, small_height;
  textures[push.small_index].GetDimensions(small_width, small_height);

  float2 texel_size = (1.0 / float2(small_width, small_height)) * push.filter_radius;
  float2 uv = (float2(input.id.xy) + 0.5) / float2(width, height);

  // 3x3 tent filter (weights 1-2-1 / 2-4-2 / 1-2-1, sum 16) softening the smaller mip as it's
  // upsampled -- same technique as the downsample's 13-tap filter, one octave up the chain.
  float3 a = sample_small(uv + texel_size * float2(-1.0, -1.0));
  float3 b = sample_small(uv + texel_size * float2( 0.0, -1.0)) * 2.0;
  float3 c = sample_small(uv + texel_size * float2( 1.0, -1.0));
  float3 d = sample_small(uv + texel_size * float2(-1.0,  0.0)) * 2.0;
  float3 e = sample_small(uv + texel_size * float2( 0.0,  0.0)) * 4.0;
  float3 f = sample_small(uv + texel_size * float2( 1.0,  0.0)) * 2.0;
  float3 g = sample_small(uv + texel_size * float2(-1.0,  1.0));
  float3 h = sample_small(uv + texel_size * float2( 0.0,  1.0)) * 2.0;
  float3 i = sample_small(uv + texel_size * float2( 1.0,  1.0));

  float3 upsampled = (a + b + c + d + e + f + g + h + i) / 16.0;

  float3 big = textures[push.big_index].SampleLevel(samplers[push.sampler_index], uv, 0.0).rgb;

  storage_images[push.output_index][input.id.xy] = float4(big + upsampled, 1.0);
}
