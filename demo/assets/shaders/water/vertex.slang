#include <libsbx/common/scene_data.slang>

struct vs_in {
  uint vertex_id : SV_VertexID;
}; // struct vs_in

struct vs_out {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float2 world_xz : WORLD_XZ;
  float4 sv_position : SV_Position;
}; // struct vs_out

struct push_data {
  float* height_data_buffer;
  float origin_x;
  float origin_z;
  float plane_size;
  float water_level;
  uint grid_vertices;
  uint grid_cells;
  float terrain_offset_x;
  float terrain_offset_z;
  float base_cell_size;
  uint vertices_x;
  uint vertices_z;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

[shader("vertex")]
vs_out main(vs_in input) {
  vs_out output;

  uint row = input.vertex_id / push.grid_vertices;
  uint column = input.vertex_id % push.grid_vertices;

  float u = float(column) / float(push.grid_cells);
  float v = float(row) / float(push.grid_cells);

  float world_x = push.origin_x + u * push.plane_size;
  float world_z = push.origin_z + v * push.plane_size;

  float3 world_pos = float3(world_x, push.water_level, world_z);

  output.position = world_pos;
  output.normal = float3(0.0, 1.0, 0.0);
  output.world_xz = float2(world_x, world_z);
  output.sv_position = mul(scene.projection, mul(scene.view, float4(world_pos, 1.0)));

  return output;
}
