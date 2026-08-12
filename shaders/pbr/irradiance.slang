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

struct cs_in {
  uint3 id : SV_DispatchThreadID;
}; // struct cs_in

static const uint sample_count = 4096u;

float3 cosine_sample_hemisphere(float2 xi) {
  float phi = 2.0 * pi * xi.x;
  float cos_theta = sqrt(1.0 - xi.y);
  float sin_theta = sqrt(xi.y);

  return float3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void compute_main(cs_in input) {
  uint width, height, layers;
  storage_cubes[push.output_index].GetDimensions(width, height, layers);

  if (input.id.x >= width || input.id.y >= height || input.id.z >= 6u) {
    return;
  }

  float2 uv = (float2(input.id.xy) + 0.5) / float2(width, height);
  float3 n = normalize(cube_direction(uv, input.id.z));

  float3 up = abs(n.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
  float3 tangent = normalize(cross(up, n));
  float3 bitangent = cross(n, tangent);

  float3 sum = float3(0, 0, 0);

  for (uint i = 0u; i < sample_count; ++i) {
    float2 xi = hammersley(i, sample_count);
    float3 h = cosine_sample_hemisphere(xi);
    float3 direction = normalize(tangent * h.x + bitangent * h.y + n * h.z);

    sum += min(cubes[push.source_index].SampleLevel(samplers[push.sampler_index], direction, 0.0).rgb, float3(firefly_clamp));
  }

  // Cosine-weighted importance sampling: the cos(theta)/pi PDF cancels the BRDF's cosine term,
  // leaving E(n) = pi * mean(L) rather than a plain average.
  float3 irradiance = pi * sum / float(sample_count);

  storage_cubes[push.output_index][input.id] = float4(irradiance, 1.0);
}
