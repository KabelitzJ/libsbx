// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <common.slang>

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

float2 integrate_brdf(float ndv, float roughness) {
  float3 v = float3(sqrt(1.0 - ndv * ndv), 0.0, ndv);
  float3 n = float3(0.0, 0.0, 1.0);
  float a = 0.0;
  float b = 0.0;

  const uint samples = 1024u;

  for (uint i = 0u; i < samples; ++i) {
    float2 xi = hammersley(i, samples);
    float3 h = importance_sample_ggx(xi, n, roughness);
    float3 l = normalize(2.0 * dot(v, h) * h - v);

    float ndl = max(l.z, 0.0);
    float ndh = max(h.z, 0.0);
    float vdh = max(dot(v, h), 0.0);

    if (ndl > 0.0) {
      float g = geometry_schlick(ndv, roughness) * geometry_schlick(ndl, roughness);
      float g_vis = (g * vdh) / (ndh * ndv);
      float fc = pow(1.0 - vdh, 5.0);

      a += (1.0 - fc) * g_vis;
      b += fc * g_vis;
    }
  }

  return float2(a, b) / float(samples);
}

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
  fs_out output;

  output.color = float4(integrate_brdf(max(input.uv.x, 1e-3), input.uv.y), 0.0, 1.0);

  return output;
}
