#ifndef BORDER_COMMON_SLANG_
#define BORDER_COMMON_SLANG_

struct edge {
  float2 start;
  float2 end;
}; // struct edge

struct push_data {
  float* height_data_buffer;
  edge* edges_buffer;
  uint vertices_x;
  uint vertices_z;
  float base_cell_size;
  float terrain_offset_x;
  float terrain_offset_z;
  float width;
  float height_offset;
  uint subdivisions;
  float4 color;
}; // struct push_data

#endif // BORDER_COMMON_SLANG_
