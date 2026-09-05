// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <descriptors.slang>

struct push_data {
  uint source_index;
  uint sampler_index;
  uint output_index;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

struct compute_input {
  uint3 id : SV_DispatchThreadID;
}; // struct compute_input

// Standard 13-tap downsample (Jorge Jimenez, "Next Generation Post Processing in Call of Duty:
// Advanced Warfare", SIGGRAPH 2014) -- a "plus" of four 2x2 bilinear box averages weighted to
// approximate a wide box filter without the aliasing a naive single 2x2 downsample would leave in
// the mip chain.
float3 sample_source(float2 uv) {
  return textures[push.source_index].SampleLevel(samplers[push.sampler_index], uv, 0.0).rgb;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void compute_main(compute_input input) {
  uint width, height;
  storage_images[push.output_index].GetDimensions(width, height);

  if (input.id.x >= width || input.id.y >= height) {
    return;
  }

  uint source_width, source_height;
  textures[push.source_index].GetDimensions(source_width, source_height);

  float2 texel_size = 1.0 / float2(source_width, source_height);
  float2 uv = (float2(input.id.xy) + 0.5) / float2(width, height);

  float3 a = sample_source(uv + texel_size * float2(-2.0, -2.0));
  float3 b = sample_source(uv + texel_size * float2( 0.0, -2.0));
  float3 c = sample_source(uv + texel_size * float2( 2.0, -2.0));
  float3 d = sample_source(uv + texel_size * float2(-1.0, -1.0));
  float3 e = sample_source(uv + texel_size * float2( 1.0, -1.0));
  float3 f = sample_source(uv + texel_size * float2(-2.0,  0.0));
  float3 g = sample_source(uv + texel_size * float2( 0.0,  0.0));
  float3 h = sample_source(uv + texel_size * float2( 2.0,  0.0));
  float3 i = sample_source(uv + texel_size * float2(-1.0,  1.0));
  float3 j = sample_source(uv + texel_size * float2( 1.0,  1.0));
  float3 k = sample_source(uv + texel_size * float2(-2.0,  2.0));
  float3 l = sample_source(uv + texel_size * float2( 0.0,  2.0));
  float3 m = sample_source(uv + texel_size * float2( 2.0,  2.0));

  float3 result = g * 0.125
    + (a + c + k + m) * 0.03125
    + (b + f + h + l) * 0.0625
    + (d + e + i + j) * 0.125;

  storage_images[push.output_index][input.id.xy] = float4(result, 1.0);
}
