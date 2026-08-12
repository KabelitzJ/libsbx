// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <common.slang>
#include <descriptors.slang>

struct push_data {
  uint source_index;
  uint sampler_index;
  uint output_index;
  float roughness;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

struct cs_in {
  uint3 id : SV_DispatchThreadID;
}; // struct cs_in

static const uint sample_count = 1024u;

float distribution_ggx(float ndh, float roughness) {
  float a = roughness * roughness;
  float a2 = a * a;
  float denom = ndh * ndh * (a2 - 1.0) + 1.0;

  return a2 / (pi * denom * denom);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void compute_main(cs_in input) {
  uint width, height, layers;
  storage_cubes[push.output_index].GetDimensions(width, height, layers);

  if (input.id.x >= width || input.id.y >= height || input.id.z >= 6u) {
    return;
  }

  uint source_width, source_height, source_levels;
  cubes[push.source_index].GetDimensions(0, source_width, source_height, source_levels);

  float2 uv = (float2(input.id.xy) + 0.5) / float2(width, height);
  float3 n = normalize(cube_direction(uv, input.id.z));
  float3 v = n;

  float3 result = float3(0, 0, 0);
  float total_weight = 0.0;

  if (push.roughness <= 0.0) {
    // Mirror reflection: no integration needed, sample the source cube directly.
    result = min(cubes[push.source_index].SampleLevel(samplers[push.sampler_index], n, 0.0).rgb, float3(firefly_clamp));
    total_weight = 1.0;
  } else {
    for (uint i = 0u; i < sample_count; ++i) {
      float2 xi = hammersley(i, sample_count);
      float3 h = importance_sample_ggx(xi, n, push.roughness);
      float3 l = normalize(2.0 * dot(v, h) * h - v);

      float ndl = max(dot(n, l), 0.0);

      if (ndl > 0.0) {
        // Lagarde filtered importance sampling: pick the source mip from the sample's solid angle
        // so low sample counts don't alias against a full-res source.
        float ndh = max(dot(n, h), 0.0);
        float vdh = max(dot(v, h), 0.0);
        float d = distribution_ggx(ndh, push.roughness);
        float pdf = d * ndh / (4.0 * vdh) + 0.0001;

        float omega_s = 1.0 / (float(sample_count) * pdf);
        float omega_p = 4.0 * pi / (6.0 * float(source_width) * float(source_width));
        float mip = max(0.5 * log2(omega_s / omega_p), 0.0);

        float3 sample_color = min(cubes[push.source_index].SampleLevel(samplers[push.sampler_index], l, mip).rgb, float3(firefly_clamp));

        result += sample_color * ndl;
        total_weight += ndl;
      }
    }

    result /= max(total_weight, 1e-3);
  }

  storage_cubes[push.output_index][input.id] = float4(result, 1.0);
}
