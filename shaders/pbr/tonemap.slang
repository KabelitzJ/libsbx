// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

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
  uint color_index;
  uint sampler_index;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]] Texture2D textures[];
[[vk::binding(1, 0)]] SamplerState samplers[];

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
vs_out vertex_main(uint vertex_id : SV_VertexID) {
  vs_out output;

  output.uv = float2((vertex_id << 1) & 2, vertex_id & 2);
  output.sv_position = float4(output.uv * float2(2.0) - float2(1.0), 0.0, 1.0);

  return output;
}

[shader("fragment")]
fs_out fragment_main(fs_in input) {
  float3 hdr = textures[push.color_index].Sample(samplers[push.sampler_index], input.uv).rgb;

  fs_out output;

  output.color = float4(tonemap_aces(hdr), 1.0);

  return output;
}
