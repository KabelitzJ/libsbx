// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

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

struct push_data {
  uint color_index;
  uint sampler_index;
  float exposure;
  uint bloom_index;
  float bloom_intensity;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

static const float3x3 input_matrix = float3x3(
  0.59719, 0.35458, 0.04823,
  0.07600, 0.90834, 0.01566,
  0.02840, 0.13383, 0.83777
);

static const float3x3 output_matrix = float3x3(
   1.60475, -0.53108, -0.07367,
  -0.10208,  1.10813, -0.00605,
  -0.00327, -0.07276,  1.07602
);

// Stephen Hill's ACES fit (RRT+ODT) with sRGB<->ACEScg color-space matrices.
float3 tonemap_aces(float3 color) {
  float3 v = mul(input_matrix, color);

  float3 a = v * (v + 0.0245786) - 0.000090537;
  float3 b = v * (0.983729 * v + 0.4329510) + 0.238081;

  return saturate(mul(output_matrix, a / b));
}

[shader("vertex")]
vertex_output vertex_main(vertex_input input) {
  float2 uv = float2((input.id << 1) & 2, input.id & 2);

  vertex_output output; 

  output.uv = uv; 
  output.sv_position = float4(uv * 2.0 - 1.0, 0.0, 1.0); 

  return output;
}

[shader("fragment")]
fragment_output fragment_main(fragment_input input) {
  float3 hdr = textures[push.color_index].Sample(samplers[push.sampler_index], input.uv).rgb;

  // Locked to mip 0 (SampleLevel, not Sample) -- bloom_upsample_index is bound against the whole
  // mip chain's default view, and this fullscreen triangle's UV derivatives shouldn't be trusted to
  // pick mip 0 on their own.
  float3 bloom = textures[push.bloom_index].SampleLevel(samplers[push.sampler_index], input.uv, 0.0).rgb;
  hdr += bloom * push.bloom_intensity;

  hdr *= exp2(push.exposure);

  fragment_output output;

  output.color = float4(tonemap_aces(hdr), 1.0);

  return output;
}
