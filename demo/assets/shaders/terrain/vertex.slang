#include <libsbx/common/scene_data.slang>

struct vs_in {
  uint vertex_id : SV_VertexID;
}; // struct vs_in

struct vs_out {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  float4 sv_position : SV_Position;
}; // struct vs_out

struct splat_weights {
  uint8_t grass;
  uint8_t dirt;
  uint8_t rock;
  uint8_t sand;
  uint8_t snow;
  uint8_t mud;
  uint8_t _padding0;
  uint8_t _padding1;
}; // struct splat_weights

struct push_data {
  float* height_data_buffer;
  splat_weights * splat_data_buffer;
  uint grid_verts;
  uint grid_cells;
  float ring_cell_size;
  float ring_origin_x;
  float ring_origin_z;
  uint ring_level;
  float base_cell_size;
  float terrain_offset_x;
  float terrain_offset_z;
  float terrain_width;
  float terrain_height;
  uint vertices_x;
  uint vertices_z;
  uint cells_x;
  uint cells_z;
  float tiling_scale;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

float sample_height_bilinear(float world_x, float world_z) {
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

  float h_top = lerp(h00, h10, fx);
  float h_bottom = lerp(h01, h11, fx);

  return lerp(h_top, h_bottom, fz);
}

[shader("vertex")]
vs_out main(vs_in input) {
  vs_out output;

  uint row = input.vertex_id / push.grid_verts;
  uint col = input.vertex_id % push.grid_verts;

  // Local position centered on the grid
  float local_x = (float(col) - float(push.grid_cells) * 0.5) * push.ring_cell_size;
  float local_z = (float(row) - float(push.grid_cells) * 0.5) * push.ring_cell_size;

  float world_x = local_x + push.ring_origin_x;
  float world_z = local_z + push.ring_origin_z;

  // Clamp to terrain bounds
  float half_width = push.terrain_width * 0.5;
  float half_height = push.terrain_height * 0.5;

  world_x = clamp(world_x, -half_width, half_width);
  world_z = clamp(world_z, -half_height, half_height);

  float h = sample_height_bilinear(world_x, world_z);

  float3 world_pos = float3(world_x, h, world_z);

  // Normal from finite differences
  float step = push.ring_cell_size;

  float h_left = sample_height_bilinear(world_x - step, world_z);
  float h_right = sample_height_bilinear(world_x + step, world_z);
  float h_down = sample_height_bilinear(world_x, world_z - step);
  float h_up = sample_height_bilinear(world_x, world_z + step);

  float3 normal = normalize(float3((h_left - h_right) / (2.0 * step), 1.0, (h_down - h_up) / (2.0 * step)));

  output.position = world_pos;
  output.normal = normal;
  output.sv_position = mul(scene.projection, mul(scene.view, float4(world_pos, 1.0)));

  return output;
}
