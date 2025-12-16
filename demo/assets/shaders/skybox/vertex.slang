#include <libsbx/math/constants.slang>
#include <libsbx/common/scene_data.slang>

struct vertex {
  float position_x;
  float position_y;
  float position_z;
}; // struct vertex

struct push_data {
  vertex* vertices;
  float4 tint;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

struct vs_in {
  uint vertex_id : SV_VertexID;
}; // struct vs_in

struct vs_out {
  float3 uv : TEXCOORD0;
  float4 color : TEXCOORD1;
  float4 sv_position : SV_Position;
}; // struct vs_out

float3 position_from_vertex(vertex v) {
  return float3(v.position_x, v.position_y, v.position_z);
}

[shader("vertex")]
vs_out main(vs_in input) {
  vs_out output;

  vertex vertex = push.vertices[input.vertex_id];
  float3 in_position = position_from_vertex(vertex);

  float3 view_rotation = mul((float3x3)scene.view, in_position);

  float4 clip = mul(scene.projection, float4(view_rotation, 1.0));
  output.sv_position = float4(clip.x, clip.y, clip.w, clip.w);

  // pass-through
  output.uv = in_position;
  output.color = scene.light_color;

  return output;
}
