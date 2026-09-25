// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef MATERIAL_CODE_MATERIAL_SLANG_
#define MATERIAL_CODE_MATERIAL_SLANG_

#include <pbr/geometry_common.slang>

// The entry points every pass requests from a custom-shader material (vertex_main/fragment_main
// for the opaque/transparent passes, depth_vertex_main/depth_fragment_main for depth-pre and
// shadows -- see render_pass.hpp's custom_pipeline_resolver), for a shading_model::shader_code
// material to include instead of writing them itself. The including file defines just
//
//   lighting_output material_fragment<let ReceivesShadows : bool>(material_context context)
//
// -- lit: fill a lit_surface (builtin_lit_surface(context.lighting) is the pbr material's own, from
// the fixed texture slots, to start from) and return evaluate_lit_surface<ReceivesShadows>(
// context.lighting, s); unlit: return a lighting_output directly. Slang resolves the call below
// against a definition later in the file.
//
// Vertex colours reach the fragment as context.lighting.color (opaque white for a mesh that sets
// none). Vertices pass through unchanged, and the depth/shadow passes don't alpha-clip. A file
// that needs either writes all four entry points itself instead, like a generated shader graph
// file does.

struct material_context {
  // World position/normal/tangent, raw uv, frame data, material/sampler indices.
  lighting_input lighting;
  material_data material;
  // lighting.uv with the material's uv_tiling/uv_offset applied.
  float2 uv;
}; // struct material_context

// geometry_common.slang's vertex_output/fragment_input plus the vertex colour.
struct code_vertex_output {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float2 uv : TEXCOORD;
  [[vk::location(3)]] float4 tangent : TANGENT;
  [[vk::location(4)]] float4 color : COLOR;
  float4 sv_position : SV_Position;
}; // struct code_vertex_output

struct code_fragment_input {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float2 uv : TEXCOORD;
  [[vk::location(3)]] float4 tangent : TANGENT;
  [[vk::location(4)]] float4 color : COLOR;
  float4 sv_position : SV_Position;
  bool is_front_face : SV_IsFrontFace;
}; // struct code_fragment_input

[shader("vertex")]
code_vertex_output vertex_main(vertex_input input) {
  vertex v = push.vertices[input.vertex_id];
  frame_data frame_data = *push.frame_data;

  transform_data instance = push.transforms[push.transform_offset + input.instance_id];

  float4 world_position = mul(instance.model, float4(v.position, 1.0));

  code_vertex_output output;
  output.position = world_position.xyz;
  output.normal = normalize(mul(instance.normal, float4(v.normal, 0.0)).xyz);
  output.tangent = float4(normalize(mul(instance.normal, float4(v.tangent.xyz, 0.0)).xyz), v.tangent.w);
  output.uv = v.uv;
  output.color = v.color;
  output.sv_position = mul(frame_data.projection, mul(frame_data.view, world_position));

  return output;
}

struct depth_vertex_output {
  float2 uv : TEXCOORD;
  float4 sv_position : SV_Position;
}; // struct depth_vertex_output

struct depth_fragment_input {
  float2 uv : TEXCOORD;
  float4 sv_position : SV_Position;
}; // struct depth_fragment_input

[shader("vertex")]
depth_vertex_output depth_vertex_main(vertex_input input) {
  vertex v = push.vertices[input.vertex_id];
  frame_data frame_data = *push.frame_data;

  transform_data instance = push.transforms[push.transform_offset + input.instance_id];

  float4 world_position = mul(instance.model, float4(v.position, 1.0));

  depth_vertex_output output;
  output.uv = v.uv;

  if (push.cascade_index == 0xFFFFFFFFu) {
    output.sv_position = mul(frame_data.projection, mul(frame_data.view, world_position));
  } else {
    output.sv_position = mul(frame_data.light_view_projections[push.cascade_index], world_position);
  }

  return output;
}

[shader("fragment")]
void depth_fragment_main(depth_fragment_input input) {
}

[shader("fragment")]
Policy::Output fragment_main<Policy : shading_policy>(code_fragment_input input) {
  material_context context;
  context.lighting.position = input.position;
  context.lighting.normal = input.normal;
  context.lighting.uv = input.uv;
  context.lighting.tangent = input.tangent;
  context.lighting.color = input.color;
  context.lighting.sv_position = input.sv_position;
  context.lighting.is_front_face = input.is_front_face;
  context.lighting.frame_data = *push.frame_data;
  context.lighting.material_index = push.material_index;
  context.lighting.sampler_index = push.sampler_index;
  context.lighting.clamp_sampler_index = push.clamp_sampler_index;
  context.material = push.frame_data->materials[push.material_index];
  context.uv = apply_uv_transform(context.material, input.uv);

  shading_input shading_input;
  shading_input.lighting = material_fragment<Policy::receives_shadows>(context);
  shading_input.depth = input.sv_position.z;

  return Policy::shade(shading_input);
}

#endif // MATERIAL_CODE_MATERIAL_SLANG_
