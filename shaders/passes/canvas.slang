// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <descriptors.slang>

struct canvas_vertex {
  float4 position;
  float4 color;
  float2 uv;
  uint texture_index;
  uint padding;
  float4 clip_rect;
}; // struct canvas_vertex

struct push_data {
  canvas_vertex* vertices;
  uint sampler_index;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

struct vertex_input {
  uint vertex_id : SV_VertexID;
}; // struct vertex_input

struct vertex_output {
  [[vk::location(0)]] float4 color : TEXCOORD0;
  [[vk::location(1)]] float2 uv : TEXCOORD1;
  [[vk::location(2)]] nointerpolation uint texture_index : TEXTURE_INDEX;
  [[vk::location(3)]] nointerpolation float4 clip_rect : TEXCOORD2;
  float4 sv_position : SV_Position;
}; // struct vertex_output

struct fragment_input {
  [[vk::location(0)]] float4 color : TEXCOORD0;
  [[vk::location(1)]] float2 uv : TEXCOORD1;
  [[vk::location(2)]] nointerpolation uint texture_index : TEXTURE_INDEX;
  [[vk::location(3)]] nointerpolation float4 clip_rect : TEXCOORD2;
  float4 sv_position : SV_Position;
}; // struct fragment_input

struct fragment_output {
  float4 color : SV_Target0;
}; // struct fragment_output

[shader("vertex")]
vertex_output vertex_main(vertex_input input) {
  canvas_vertex vertex = push.vertices[input.vertex_id];

  vertex_output output;

  output.color = vertex.color;
  output.uv = vertex.uv;
  output.texture_index = vertex.texture_index;
  output.clip_rect = vertex.clip_rect;
  output.sv_position = vertex.position;

  return output;
}

bool clipped(fragment_input input) {
  return input.sv_position.x < input.clip_rect.x || input.sv_position.x > input.clip_rect.z ||
         input.sv_position.y < input.clip_rect.y || input.sv_position.y > input.clip_rect.w;
}

[shader("fragment")]
fragment_output fragment_main(fragment_input input) {
  if (clipped(input)) {
    discard;
  }

  fragment_output output;

  output.color = input.color * textures[input.texture_index].Sample(samplers[push.sampler_index], input.uv);

  return output;
}

[shader("fragment")]
fragment_output fragment_main_text(fragment_input input) {
  if (clipped(input)) {
    discard;
  }

  // Always mip 0 (font atlases are uploaded without mips anyway -- asset_residency's
  // pending_texture_upload::mipmapped): a distance field can't be averaged -- a thin stem's (l, i)
  // peak distance, averaged with the background around it, drops under the 0.5 edge and the stroke
  // vanishes.
  //
  // Instead, 2x2 samples across the pixel's footprint in the atlas, coverage averaged: small text
  // shrinks the atlas several times over, so one sample per pixel either hits a thin stem or misses
  // it depending on where the glyph falls -- strokes flickered in and out. Averaged, a stem thinner
  // than a pixel still gives it partial coverage. Each sample covers half a pixel, so its edge
  // softening is half the pixel's distance change.
  float2 dx = ddx(input.uv) * 0.25;
  float2 dy = ddy(input.uv) * 0.25;

  float center = textures[input.texture_index].SampleLevel(samplers[push.sampler_index], input.uv, 0.0).r;
  float width = fwidth(center) * 0.5;

  float alpha = 0.0;

  [unroll]
  for (int i = 0; i < 4; ++i) {
    float2 offset = ((i & 1) != 0 ? dx : -dx) + ((i & 2) != 0 ? dy : -dy);
    float distance = textures[input.texture_index].SampleLevel(samplers[push.sampler_index], input.uv + offset, 0.0).r;

    alpha += smoothstep(0.5 - width, 0.5 + width, distance) * 0.25;
  }

  fragment_output output;

  output.color = float4(input.color.rgb, input.color.a * alpha);

  return output;
}
