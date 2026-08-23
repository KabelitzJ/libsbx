// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <common.slang>
#include <descriptors.slang>

struct push_data {
  uint output_index;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

struct cs_in {
  uint3 id : SV_DispatchThreadID;
}; // struct cs_in

static const uint sample_count = 1024u;

float2 integrate_brdf(float ndv, float roughness) {
  float3 v = float3(sqrt(1.0 - ndv * ndv), 0.0, ndv);
  float3 n = float3(0.0, 0.0, 1.0);
  float a = 0.0;
  float b = 0.0;

  for (uint i = 0u; i < sample_count; ++i) {
    float2 xi = hammersley(i, sample_count);
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

  return float2(a, b) / float(sample_count);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void compute_main(cs_in input) {
  uint width, height;
  storage_images[push.output_index].GetDimensions(width, height);

  if (input.id.x >= width || input.id.y >= height) {
    return;
  }

  float2 uv = (float2(input.id.xy) + 0.5) / float2(width, height);
  float2 result = integrate_brdf(max(uv.x, 1e-3), uv.y);

  storage_images[push.output_index][input.id.xy] = float4(result, 0.0, 1.0);
}
