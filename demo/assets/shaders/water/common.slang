#ifndef WATER_COMMON_SLANG_
#define WATER_COMMON_SLANG_

struct push_data {
  float* height_data_buffer;
  float water_y;
  float bounds_min_x;
  float bounds_min_z;
  float bounds_max_x;
  float bounds_max_z;
  float roughness;
  float4 water_color_shallow;
  float4 water_color_deep;
  float depth_fade_distance;
  uint grid_size;
  float wave_amplitude;
  float wave_frequency;
  float wave_speed;
  uint vertices_x;
  uint vertices_z;
  float terrain_offset_x;
  float terrain_offset_z;
  float base_cell_size;
}; // struct push_data

#endif // WATER_COMMON_SLANG_
