// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <common.slang>
#include <descriptors.slang>

struct vs_in { 
  uint id : SV_VertexID; 
}; // struct vs_in

struct vs_out { 
  [[vk::location(0)]] float2 uv : TEXCOORD; 
  float4 sv_position : SV_Position; 
}; // struct vs_out

struct fs_in { 
  [[vk::location(0)]] float2 uv : TEXCOORD; 
}; // struct fs_in

struct fs_out { 
  float4 color : SV_Target0; 
}; // struct fs_out

struct push_data { 
  uint environment_index; 
  uint sampler_index; 
  float roughness; 
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

[shader("vertex")]
vs_out vertex_main(vs_in input) {
  float2 uv = float2((input.id << 1) & 2, input.id & 2);

  vs_out output; 

  output.uv = uv; 
  output.sv_position = float4(uv * 2.0 - 1.0, 0.0, 1.0); 

  return output;
}

[shader("fragment")]
fs_out fragment_main(fs_in input) {
  float3 n = equirect_direction(input.uv);
  float3 v = n;

  float3 prefiltered = float3(0, 0, 0);
  float weight = 0.0;
  const uint samples = 1024u;

  for (uint i = 0u; i < samples; ++i) {
    float2 xi = hammersley(i, samples);
    float3 h = importance_sample_ggx(xi, n, push.roughness);
    float3 l = normalize(2.0 * dot(v, h) * h - v);

    float ndl = max(dot(n, l), 0.0);

    if (ndl > 0.0) {
      float3 radiance = min(textures[push.environment_index].SampleLevel(samplers[push.sampler_index], direction_to_equirect(l), 0.0).rgb, float3(firefly_clamp));
      prefiltered += radiance * ndl;
      weight += ndl;
    }
  }

  fs_out output;

  output.color = float4(prefiltered / max(weight, 1e-3), 1.0);

  return output;
}
