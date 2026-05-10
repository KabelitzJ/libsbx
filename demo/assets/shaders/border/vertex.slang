#include <libsbx/common/scene_data.slang>

#include <border/common.slang>

struct vs_in {
  uint vertex_id : SV_VertexID;
  uint instance_id : SV_InstanceID;
}; // struct vs_in

struct vs_out {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float2 ribbon_uv : TEXCOORD0;
  float4 sv_position : SV_Position;
}; // struct vs_out

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

float sample_terrain_height(float world_x, float world_z) {
  float grid_x = (world_x + push.terrain_offset_x) / push.base_cell_size;
  float grid_z = (world_z + push.terrain_offset_z) / push.base_cell_size;

  int x0 = int(floor(grid_x));
  int z0 = int(floor(grid_z));
  int x1 = x0 + 1;
  int z1 = z0 + 1;

  x0 = clamp(x0, 0, int(push.vertices_x) - 1);
  x1 = clamp(x1, 0, int(push.vertices_x) - 1);
  z0 = clamp(z0, 0, int(push.vertices_z) - 1);
  z1 = clamp(z1, 0, int(push.vertices_z) - 1);

  float fx = grid_x - floor(grid_x);
  float fz = grid_z - floor(grid_z);

  float h00 = push.height_data_buffer[z0 * push.vertices_x + x0];
  float h10 = push.height_data_buffer[z0 * push.vertices_x + x1];
  float h01 = push.height_data_buffer[z1 * push.vertices_x + x0];
  float h11 = push.height_data_buffer[z1 * push.vertices_x + x1];

  return lerp(lerp(h00, h10, fx), lerp(h01, h11, fx), fz);
}

[shader("vertex")]
vs_out main(vs_in input) {
  // Each edge is divided into `subdivisions` sub-quads (6 verts each).
  // sub_index identifies which sub-quad this vertex belongs to.
  // local_id identifies which of the 6 verts inside that sub-quad.

  uint sub_index = input.vertex_id / 6;
  uint local_id  = input.vertex_id % 6;

  edge e = push.edges_buffer[input.instance_id];
  float2 start = e.start;
  float2 end = e.end;

  float2 dir = end - start;
  float len = max(length(dir), 1e-5);
  dir = dir / len;

  float2 perp = float2(-dir.y, dir.x);
  float half_w = push.width * 0.5;

  float t0 = float(sub_index)     / float(push.subdivisions);
  float t1 = float(sub_index + 1) / float(push.subdivisions);

  // sub-quad triangle pattern (CCW from +Y):
  // 0: t0, across=-1   1: t1, across=-1   2: t0, across=+1
  // 3: t1, across=-1   4: t1, across=+1   5: t0, across=+1
  float along = 0.0;
  float across = 0.0;

  switch (local_id) {
    case 0: along = t0; across = -1.0; break;
    case 1: along = t1; across = -1.0; break;
    case 2: along = t0; across =  1.0; break;
    case 3: along = t1; across = -1.0; break;
    case 4: along = t1; across =  1.0; break;
    case 5: along = t0; across =  1.0; break;
  }

  float2 base = lerp(start, end, along);
  float2 world_xz = base + perp * across * half_w;

  float terrain_h = sample_terrain_height(world_xz.x, world_xz.y);
  float3 world_position = float3(world_xz.x, terrain_h + push.height_offset, world_xz.y);

  vs_out output;

  output.position = world_position;
  output.normal = float3(0.0, 1.0, 0.0);
  output.ribbon_uv = float2(along, across);
  output.sv_position = mul(scene.projection, mul(scene.view, float4(world_position, 1.0)));

  return output;
}
