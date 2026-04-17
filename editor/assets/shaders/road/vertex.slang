#include <libsbx/common/scene_data.slang>

struct vs_in {
  uint vertex_id : SV_VertexID;
}; // struct vs_in

struct vs_out {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float2 uv : TEXCOORD;
  [[vk::location(2)]] nointerpolation uint road_type : ROAD_TYPE;
  float4 sv_position : SV_Position;
}; // struct vs_out

struct road_vertex_data {
  float position_x;
  float position_y;
  float position_z;
  float uv_u;
  float uv_v;
  uint road_type;
}; // struct road_vertex_data

struct push_data {
  road_vertex_data* vertex_data_buffer;
  uint preview_mode;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

[shader("vertex")]
vs_out main(vs_in input) {
  vs_out output;

  road_vertex_data vertex = push.vertex_data_buffer[input.vertex_id];

  float3 world_position = float3(vertex.position_x, vertex.position_y, vertex.position_z);

  output.position = world_position;
  output.uv = float2(vertex.uv_u, vertex.uv_v);
  output.road_type = vertex.road_type;
  output.sv_position = mul(scene.projection, mul(scene.view, float4(world_position, 1.0)));

  return output;
}
