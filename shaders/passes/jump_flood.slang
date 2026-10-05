// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

// Jump flood selection outline (selection_outline_pass), ported from Hazel's JumpFlood_Init/_Pass/_Composite.
// Each texel holds (offset to the nearest texel on the other side of the silhouette, its squared distance,
// side), with x measured in v units so distances are aspect correct:
//   init_main:      side = mask alpha > 0.5, no neighbour found yet.
//   flood_main:     looks `step` texels away in the 8 directions and keeps the nearest other-side texel.
//   composite_main: orange where that distance is under ~2 texels at 1080p, fading out by ~4.

#include <descriptors.slang>

struct vertex_input {
  uint id : SV_VertexID;
}; // struct vertex_input

struct vertex_output {
  [[vk::location(0)]] float2 uv : TEXCOORD;
  float4 sv_position : SV_Position;
}; // struct vertex_output

struct fragment_input {
  [[vk::location(0)]] float2 uv : TEXCOORD;
}; // struct fragment_input

struct fragment_output {
  float4 color : SV_Target0;
}; // struct fragment_output

// Mirrors selection_outline_pass.cpp's jump_flood_push.
struct push_data {
  uint texture_index;
  uint sampler_index;
  float texel_size_x;
  float texel_size_y;
  int step;
  float color_r;     // linear; three floats, not a float3 -- that would be 16-byte aligned
  float color_g;
  float color_b;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

[shader("vertex")]
vertex_output vertex_main(vertex_input input) {
  float2 uv = float2((input.id << 1) & 2, input.id & 2);

  vertex_output output;

  output.uv = uv;
  output.sv_position = float4(uv * 2.0 - 1.0, 0.0, 1.0);

  return output;
}

float4 sample_input(float2 uv) {
  return textures[push.texture_index].SampleLevel(samplers[push.sampler_index], uv, 0.0);
}

float screen_distance(float2 v) {
  float ratio = push.texel_size_x / push.texel_size_y;
  v.x /= ratio;
  return dot(v, v);
}

[shader("fragment")]
fragment_output init_main(fragment_input input) {
  float4 color = sample_input(input.uv);

  float4 result;
  result.xy = float2(100.0, 100.0);
  result.z = screen_distance(result.xy);
  result.w = color.a > 0.5 ? 1.0 : 0.0; // inside vs. outside

  fragment_output output;

  output.color = result;

  return output;
}

[shader("fragment")]
fragment_output flood_main(fragment_input input) {
  float2 dx = float2(push.texel_size_x, 0.0) * float(push.step);
  float2 dy = float2(0.0, push.texel_size_y) * float(push.step);

  float2 offsets[8] = { dx, -dx, dy, -dy, dx + dy, dx - dy, -dx + dy, -dx - dy };

  float4 pixel = sample_input(input.uv);

  for (int j = 0; j < 8; ++j) {
    float2 uv = input.uv + offsets[j];

    // A neighbour on the other side is itself the nearest other-side texel; one on our side passes on its own.
    float4 neighbour = sample_input(uv);

    if (neighbour.w != pixel.w) {
      neighbour.xyz = float3(0.0);
    }

    neighbour.xy += offsets[j];

    if (any(uv < 0.0) || any(uv > 1.0)) {
      neighbour.xy = float2(1000.0);
    }

    float distance = screen_distance(neighbour.xy);

    if (distance < pixel.z) {
      pixel.xyz = float3(neighbour.xy, distance);
    }
  }

  fragment_output output;

  output.color = pixel;

  return output;
}

[shader("fragment")]
fragment_output composite_main(fragment_input input) {
  float4 pixel = sample_input(input.uv);

  float distance = sqrt(pixel.z);
  float alpha = smoothstep(0.004, 0.002, distance);

  if (alpha == 0.0) {
    discard;
  }

  fragment_output output;

  output.color = float4(push.color_r, push.color_g, push.color_b, alpha);

  return output;
}
