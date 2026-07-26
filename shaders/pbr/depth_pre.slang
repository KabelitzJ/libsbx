// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

struct vs_in {
  uint vertex_id : SV_VertexID;
  uint instance_id : SV_InstanceID;
}; // struct vs_in

struct vs_out {
  float4 sv_position : SV_Position;
}; // struct vs_out

struct vertex {
  float3 position;
  float3 normal;
  float2 uv;
  float4 tangent;
}; // struct vertex

struct frame_data {
  float4x4 view;
  float4x4 projection;
}; // struct frame_data

struct push_data {
  frame_data* frame;
  vertex* vertices;
  float4x4* transforms;
  uint transform_offset;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

[shader("vertex")]
vs_out vertex_main(vs_in input) {
  vertex v = push.vertices[input.vertex_id];
  frame_data frame = push.frame[0];

  float4x4 model = push.transforms[push.transform_offset + input.instance_id];
  float4 world = mul(model, float4(v.position, 1.0));

  vs_out output;

  output.sv_position = mul(frame.projection, mul(frame.view, world));

  return output;
}

[shader("fragment")]
void fragment_main() {

}
