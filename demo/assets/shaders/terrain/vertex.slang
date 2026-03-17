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
  uint chunk_verts;
  uint chunk_cells;
  float cell_size;
  float chunk_world_x;
  float chunk_world_z;
  float inv_chunk_verts;
  float tiling_scale;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]]
ConstantBuffer<sbx::common::scene_data> scene;

float sample_height(int x, int y) {
  uint cx = uint(clamp(x, 0, int(push.chunk_verts) - 1));
  uint cy = uint(clamp(y, 0, int(push.chunk_verts) - 1));

  return push.height_data_buffer[cy * push.chunk_verts + cx];
}

[shader("vertex")]
vs_out main(vs_in input) {
  vs_out output;

  uint y = uint(float(input.vertex_id) * push.inv_chunk_verts);
  uint x = input.vertex_id - y * push.chunk_verts;

  float h = push.height_data_buffer[input.vertex_id];

  float3 world_pos = float3(float(x) * push.cell_size + push.chunk_world_x, h, float(y) * push.cell_size + push.chunk_world_z);

  float h_left  = sample_height(int(x) - 1, int(y));
  float h_right = sample_height(int(x) + 1, int(y));
  float h_down  = sample_height(int(x), int(y) - 1);
  float h_up    = sample_height(int(x), int(y) + 1);

  float step = 2.0 * push.cell_size;

  float3 normal = normalize(float3((h_left - h_right) / step, 1.0, (h_down - h_up) / step));

  output.position = world_pos;
  output.normal = normal;
  output.sv_position = mul(scene.projection, mul(scene.view, float4(world_pos, 1.0)));

  return output;
}