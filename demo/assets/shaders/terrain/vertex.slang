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
  splat_weights* splat_data_buffer;
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
  float inner_min_x;
  float inner_min_z;
  float inner_max_x;
  float inner_max_z;
  uint ring_count;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

struct height_sample {
  float height;
  float dh_dx;
  float dh_dz;
}; // struct height_sample

height_sample sample_height_and_gradient(float world_x, float world_z) {
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

  height_sample result;
  result.height = lerp(lerp(h00, h10, fx), lerp(h01, h11, fx), fz);

  // Bilinear patch partial derivatives: dh/du, dh/dv in grid space
  // Convert to world space by dividing by base_cell_size
  float inv_cell = 1.0 / push.base_cell_size;
  result.dh_dx = ((1.0 - fz) * (h10 - h00) + fz * (h11 - h01)) * inv_cell;
  result.dh_dz = ((1.0 - fx) * (h01 - h00) + fx * (h11 - h10)) * inv_cell;

  return result;
}

float sample_height_coarser(float world_x, float world_z) {
  float coarser_step = push.ring_cell_size * 2.0 / push.base_cell_size;

  float gx = (world_x + push.terrain_offset_x) / push.base_cell_size;
  float gz = (world_z + push.terrain_offset_z) / push.base_cell_size;

  float cgx = floor(gx / coarser_step) * coarser_step;
  float cgz = floor(gz / coarser_step) * coarser_step;

  float fx = (gx - cgx) / coarser_step;
  float fz = (gz - cgz) / coarser_step;

  int ix0 = clamp(int(cgx), 0, int(push.vertices_x) - 1);
  int iz0 = clamp(int(cgz), 0, int(push.vertices_z) - 1);
  int ix1 = clamp(int(cgx + coarser_step), 0, int(push.vertices_x) - 1);
  int iz1 = clamp(int(cgz + coarser_step), 0, int(push.vertices_z) - 1);

  float h00 = push.height_data_buffer[iz0 * push.vertices_x + ix0];
  float h10 = push.height_data_buffer[iz0 * push.vertices_x + ix1];
  float h01 = push.height_data_buffer[iz1 * push.vertices_x + ix0];
  float h11 = push.height_data_buffer[iz1 * push.vertices_x + ix1];

  return lerp(lerp(h00, h10, fx), lerp(h01, h11, fx), fz);
}

vs_out degenerate() {
  vs_out output;
  output.sv_position = float4(0.0, 0.0, 2.0, 1.0);
  output.position = float3(0.0, 0.0, 0.0);
  output.normal = float3(0.0, 1.0, 0.0);
  return output;
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

  // Inner cutout: collapse vertices covered by the finer ring
  // Shrink cutout by one inner cell on each side to guarantee overlap (no gaps)
  if (push.ring_level > 0) {
    float margin = push.ring_cell_size * 0.5;

    if (world_x > (push.inner_min_x + margin) &&
        world_x < (push.inner_max_x - margin) &&
        world_z > (push.inner_min_z + margin) &&
        world_z < (push.inner_max_z - margin)) {
      return degenerate();
    }
  }

  // T-junction stitching: snap outer boundary vertices to the coarser grid
  // Creates zero-area triangles that seal gaps between this ring and the next coarser
  bool is_outermost_ring = (push.ring_level == push.ring_count - 1);

  if (!is_outermost_ring) {
    float coarser_cell = push.ring_cell_size * 2.0;

    if (row == 0 || row == push.grid_cells) {
      world_x = round(world_x / coarser_cell) * coarser_cell;
    }

    if (col == 0 || col == push.grid_cells) {
      world_z = round(world_z / coarser_cell) * coarser_cell;
    }

    // Re-clamp after snapping
    world_x = clamp(world_x, -half_width, half_width);
    world_z = clamp(world_z, -half_height, half_height);
  }

  // Transition alpha: ramps from 0 in interior to 1 at outer boundary
  float alpha = 0.0;

  if (!is_outermost_ring) {
    float half_extent = float(push.grid_cells) * push.ring_cell_size * 0.5;
    float transition_width = float(push.grid_cells) * push.ring_cell_size * 0.1;

    float dx = abs(world_x - push.ring_origin_x);
    float dz = abs(world_z - push.ring_origin_z);

    float alpha_x = clamp((dx - (half_extent - transition_width)) / transition_width, 0.0, 1.0);
    float alpha_z = clamp((dz - (half_extent - transition_width)) / transition_width, 0.0, 1.0);

    alpha = max(alpha_x, alpha_z);
  }

  // Sample height and gradient in one pass (4 buffer reads total)
  height_sample hs = sample_height_and_gradient(world_x, world_z);
  float h = hs.height;

  if (alpha > 0.0) {
    float z_coarse = sample_height_coarser(world_x, world_z);
    h = lerp(h, z_coarse, alpha);
  }

  float3 world_pos = float3(world_x, h, world_z);

  // Analytical normal from bilinear patch partial derivatives
  float3 normal = normalize(float3(-hs.dh_dx, 1.0, -hs.dh_dz));

  output.position = world_pos;
  output.normal = normal;
  output.sv_position = mul(scene.projection, mul(scene.view, float4(world_pos, 1.0)));

  return output;
}
