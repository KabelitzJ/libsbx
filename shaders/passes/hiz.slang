// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <common.slang>
#include <descriptors.slang>

// One level of the depth pyramid: every texel holds the farthest depth among the source texels it covers.
// Level 0 reads the scene depth (any size, scaled to the pyramid), each later level the one before it.
// Taking the farthest keeps occlusion tests conservative: nothing behind a texel's value can be seen through it.

struct push_data {
  uint source_index;      // sampled image: the scene depth for level 0, the pyramid itself after that
  uint destination_index; // storage image: this level's view
  uint source_level;
  uint padding;
  uint2 source_size;
  uint2 size;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[shader("compute")]
[numthreads(8, 8, 1)]
void compute_main(uint3 id : SV_DispatchThreadID) {
  if (id.x >= push.size.x || id.y >= push.size.y) {
    return;
  }

  // The source rect this texel covers, rounded outward so no source texel is left out.
  float2 scale = float2(push.source_size) / float2(push.size);
  uint2 begin = min(uint2(floor(float2(id.xy) * scale)), push.source_size - 1u);
  uint2 end = clamp(uint2(ceil(float2(id.xy + 1u) * scale)), begin + 1u, push.source_size);

  Texture2D source = textures[push.source_index];

  float farthest = 0.0;

  for (uint y = begin.y; y < end.y; ++y) {
    for (uint x = begin.x; x < end.x; ++x) {
      farthest = max(farthest, source.Load(int3(int(x), int(y), int(push.source_level))).r);
    }
  }

  storage_images[push.destination_index][id.xy] = float4(farthest, 0.0, 0.0, 0.0);
}
