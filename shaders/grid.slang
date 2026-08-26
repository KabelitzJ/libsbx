// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <frame_data.slang>

static const float grid_size = 500.0;
static const float grid_cell_size = 1.0;
static const float4 grid_color_thick = float4(1.0, 1.0, 1.0, 1.0);
static const float4 grid_color_thin = float4(0.5, 0.5, 0.5, 1.0);
static const float grid_min_pixels_between_cells = 2.0;

static const float3 grid_axis_color_x = float3(0.85, 0.15, 0.15);
static const float3 grid_axis_color_z = float3(0.2, 0.4, 0.9);
static const float grid_axis_half_width_px = 1.5;

struct push_data {
  frame_data* frame;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

struct vertex_input {
  uint vertex_id : SV_VertexID;
}; // struct vertex_input

struct vertex_output {
  [[vk::location(0)]] float2 uv : TEXCOORD0;
  [[vk::location(1)]] float2 camera_position : TEXCOORD1;
  float4 sv_position : SV_Position;
}; // struct vertex_output

struct fragment_input {
  [[vk::location(0)]] float2 uv : TEXCOORD0;
  [[vk::location(1)]] float2 camera_position : TEXCOORD1;
}; // struct fragment_input

struct fragment_output {
  float4 color : SV_Target0;
}; // struct fragment_output

static const float3 grid_vertices[4] = {
  float3(-1.0, 0.0, -1.0),
  float3( 1.0, 0.0, -1.0),
  float3( 1.0, 0.0,  1.0),
  float3(-1.0, 0.0,  1.0)
};

static const uint grid_indices[6] = { 0u, 1u, 2u, 2u, 3u, 0u };

[shader("vertex")]
vertex_output vertex_main(vertex_input input) {
  frame_data frame = *push.frame;

  float3 position = grid_vertices[grid_indices[input.vertex_id]] * grid_size;
  position.x += frame.camera_position.x;
  position.z += frame.camera_position.z;

  vertex_output output;

  output.camera_position = frame.camera_position.xz;
  output.uv = position.xz;
  output.sv_position = mul(frame.projection, mul(frame.view, float4(position, 1.0)));

  return output;
}

float grid_log10(float x) { 
  return log(x) / log(10.0); 
}

float grid_satf(float x) { 
  return clamp(x, 0.0, 1.0); 
}

float2 grid_satv(float2 x) { 
  return clamp(x, float2(0.0, 0.0), float2(1.0, 1.0)); 
}

float grid_max2(float2 v) { 
  return max(v.x, v.y); 
}

float2 grid_mod(float2 x, float y) { 
  return x - y * floor(x / y); 
}

float4 grid_color(float2 uv, float2 camera_position) {
  float2 dudv = float2(length(float2(ddx(uv.x), ddy(uv.x))), length(float2(ddx(uv.y), ddy(uv.y))));

  float lod_level = max(0.0, grid_log10((length(dudv) * grid_min_pixels_between_cells) / grid_cell_size) + 1.0);
  float lod_fade = frac(lod_level);

  float lod0 = grid_cell_size * pow(10.0, floor(lod_level));
  float lod1 = lod0 * 10.0;
  float lod2 = lod1 * 10.0;

  float2 axis_coverage = grid_satv(float2(grid_axis_half_width_px, grid_axis_half_width_px) - abs(uv) / dudv);

  dudv *= 4.0;
  uv += dudv * 0.5;

  float lod0a = grid_max2(float2(1.0, 1.0) - abs(grid_satv(grid_mod(uv, lod0) / dudv) * 2.0 - float2(1.0, 1.0)));
  float lod1a = grid_max2(float2(1.0, 1.0) - abs(grid_satv(grid_mod(uv, lod1) / dudv) * 2.0 - float2(1.0, 1.0)));
  float lod2a = grid_max2(float2(1.0, 1.0) - abs(grid_satv(grid_mod(uv, lod2) / dudv) * 2.0 - float2(1.0, 1.0)));

  float4 color = (lod2a > 0.0) ? grid_color_thick : (lod1a > 0.0) ? lerp(grid_color_thick, grid_color_thin, lod_fade) : grid_color_thin;
  float grid_alpha = (lod2a > 0.0) ? lod2a : (lod1a > 0.0) ? lod1a : (lod0a * (1.0 - lod_fade));

  color.rgb = lerp(color.rgb, grid_axis_color_x, axis_coverage.y);
  color.rgb = lerp(color.rgb, grid_axis_color_z, axis_coverage.x);
  float alpha = max(grid_alpha, grid_max2(axis_coverage));

  uv -= camera_position;
  float opacity_falloff = 1.0 - grid_satf(length(uv) / grid_size);

  color.a = alpha * opacity_falloff;

  return color;
}

[shader("fragment")]
fragment_output fragment_main(fragment_input input) {
  fragment_output output;

  output.color = grid_color(input.uv, input.camera_position);

  return output;
}
