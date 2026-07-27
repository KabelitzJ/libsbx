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

  float3 up = abs(n.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);
  float3 right = normalize(cross(up, n));
  up = cross(n, right);

  float3 irradiance = float3(0, 0, 0);
  float samples = 0.0;
  const float delta = 0.05;

  for (float phi = 0.0; phi < 2.0 * pi; phi += delta) {
    for (float theta = 0.0; theta < 0.5 * pi; theta += delta) {
      float3 tangent = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
      float3 dir = tangent.x * right + tangent.y * up + tangent.z * n;

      float3 radiance = min(textures[push.environment_index].SampleLevel(samplers[push.sampler_index], direction_to_equirect(dir), 0.0).rgb, float3(firefly_clamp));
      irradiance += radiance * cos(theta) * sin(theta);

      samples += 1.0;
    }
  }

  fs_out output;

  output.color = float4(pi * irradiance / samples, 1.0);

  return output;
}
