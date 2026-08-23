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
  float3 v = n;

  float3 result = float3(0, 0, 0);
  float total_weight = 0.0;

  // Always sample the source cube's own mip 0, at every roughness — no solid-angle-driven mip
  // selection (Lagarde et al.'s filtered importance sampling). That technique trades noise for
  // reading progressively lower mips of the source cube, which only pays off if those mips are
  // trustworthy; ours come from a naive per-face-independent blit of a runtime-synthesized cube
  // (see generate_mip_chain), so low mips can carry face-seam inconsistency that this convolution
  // would otherwise bake permanently into the prefiltered result — worst exactly at high
  // roughness, where the widest GGX lobes read the lowest, most seam-prone mips. Sampling mip 0
  // unconditionally avoids that; at sample_count = 1024 it's still clean. This matches the
  // reference IBL implementation's prefilter shader, which never selects a source mip either.
  for (uint i = 0u; i < sample_count; ++i) {
    float2 xi = hammersley(i, sample_count);
    float3 h = importance_sample_ggx(xi, n, push.roughness);
    float3 l = normalize(2.0 * dot(v, h) * h - v);

    float ndl = max(dot(n, l), 0.0);

    if (ndl > 0.0) {
      float3 sample_color = min(cubes[push.source_index].SampleLevel(samplers[push.sampler_index], l, 0.0).rgb, float3(firefly_clamp));

      result += sample_color * ndl;
      total_weight += ndl;
    }
  }

  result /= max(total_weight, 1e-3);

  storage_cubes[push.output_index][input.id] = float4(result, 1.0);
}
