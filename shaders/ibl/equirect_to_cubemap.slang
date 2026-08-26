// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <common.slang>
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

[shader("compute")]
[numthreads(8, 8, 1)]
void compute_main(compute_input input) {
  uint width, height, layers;
  storage_cubes[push.output_index].GetDimensions(width, height, layers);

  if (input.id.x >= width || input.id.y >= height || input.id.z >= 6u) {
    return;
  }

  float2 uv = (float2(input.id.xy) + 0.5) / float2(width, height);
  float3 direction = normalize(cube_direction(uv, input.id.z));

  float3 color = textures[push.source_index].SampleLevel(samplers[push.sampler_index], direction_to_equirect(direction), 0.0).rgb;

  storage_cubes[push.output_index][input.id] = float4(color, 1.0);
}
