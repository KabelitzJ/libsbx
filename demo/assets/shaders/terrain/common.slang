#ifndef TERRAIN_COMMON_SLANG_
#define TERRAIN_COMMON_SLANG_

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
  uint* province_data_buffer;

  // ring & terrain layout
  uint grid_verts;
  uint grid_cells;
  uint ring_level;
  uint ring_count;

  float ring_cell_size;
  float ring_origin_x;
  float ring_origin_z;
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

  // looks
  float tiling_scale;
  uint selected_province_id;
  float selection_strength;
  float _pad0;

  float3 selection_color;
  float _pad1;
}; // struct push_data

#endif // TERRAIN_COMMON_SLANG_
